/* Native KH1 HUD adapter. Steam Global 1.0.0.2 only.
 * EXP, stats, rewards and save bytes remain read-only.
 * One guarded HUD call is redirected; vanilla notifications pass through.
 * An atomic frame-vtable hook chains existing helpers and calls the native
 * notification queue only on the game's frame thread.
 * Native queue layout reference: gaithern/KH1-LUA-LIBRARY (MIT),
 * modules/level_up_prompt.lua, original technique credited there to Topaz.
 * All addresses/layouts additionally checked against the supported executable.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "counter.h"
#include "hud.h"

typedef uint64_t (__cdecl *FrameProc)(void *);
typedef void *(__fastcall *ResolveProc)(uint32_t);
typedef void (__fastcall *EnqueueProc)(void *, const void *, const void *);
static uintptr_t game;
static FrameProc original_frame;
static NextLevelCounter counter;
static volatile LONG64 lease;
static int enabled, failed, was_f6;
static uintptr_t owned_box;
/* Static, private buffers survive Lua reload and every native animation. */
static unsigned char title[32], body[32];
static int32_t purple_box[4] = {94, 56, 156, 128};
static int32_t purple_text[4] = {190, 156, 255, 128};

__declspec(dllexport) volatile uint32_t nextlevel_build = 102;
__declspec(dllexport) volatile uint32_t nextlevel_status; /* 0 waiting, 1 ready, 2 refused */
__declspec(dllexport) volatile uint32_t nextlevel_frames;
__declspec(dllexport) volatile uint32_t nextlevel_popups;
__declspec(dllexport) volatile uint32_t nextlevel_exp;
__declspec(dllexport) volatile uint32_t nextlevel_remaining;
__declspec(dllexport) volatile uint32_t nextlevel_level;
__declspec(dllexport) volatile uint32_t nextlevel_pace;
__declspec(dllexport) volatile uint32_t nextlevel_visible;
__declspec(dllexport) volatile uint32_t nextlevel_deferred;

static int readable(uintptr_t p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    return p && n && p+n >= p && VirtualQuery((void *)p, &m, sizeof(m)) &&
        m.State == MEM_COMMIT && !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        p+n <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
static uint32_t u32(uintptr_t p) { return *(uint32_t *)p; }
static uintptr_t pointer(uintptr_t p) { return *(uintptr_t *)p; }
static int bytes_at(uintptr_t rva, const unsigned char *b, size_t n) {
    return readable(game+rva,n) && !memcmp((void *)(game+rva),b,n);
}
static void report(const char *s) { printf("[Next Level Popup] %s\n", s); fflush(stdout); }

static int supported(void) {
    static const unsigned char enqueue[] = {
        0x48,0x89,0x5C,0x24,8,0x48,0x89,0x6C,0x24,0x10,
        0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20};
    static const unsigned char resolve[] = {0x85,0xC9,0x75,3,0x33,0xC0,0xC3,0xE9,0x74,1,0,0};
    static const unsigned char exp_table[] = {0x48,0x63,0xC1,0x48,0x8D,0x0D,0xE6,0x33,0xA9,0x02};
    if (!game || !readable(game,sizeof(IMAGE_DOS_HEADER))) return 0;
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)game;
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000)
        return 0;
    uintptr_t nt_address=game+(unsigned)dos->e_lfanew;
    if (!readable(nt_address,sizeof(IMAGE_NT_HEADERS64))) return 0;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64 *)nt_address;
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.SizeOfImage<0x2EE3998) return 0;
    return readable(game+0x2EE3990,8) && *(unsigned char *)(game+0x4698D2)==106 &&
        u32(game+0x3EA388)==540680280 && *(unsigned char *)(game+0x26E20C)==9 &&
        bytes_at(0x272540,enqueue,sizeof(enqueue)) &&
        bytes_at(0x38ADC0,resolve,sizeof(resolve)) &&
        bytes_at(0x28F950,exp_table,sizeof(exp_table)) &&
        bytes_at(0x27311A,(const unsigned char[]){0xE8,0x11,0xF8,0xFF,0xFF},5) &&
        bytes_at(0x2D2840,(const unsigned char[]){0x48,0x83,0xEC,0x58,0x66,0x0F,0x6E,0x84,0x24,0x80,0,0,0,0x66,0x0F,0x6E,0x8C,0x24,0x88,0},20) &&
        bytes_at(0x2D1CD0,(const unsigned char[]){0x40,0x53,0x48,0x83,0xEC,0x50,0x48,0x8B},8) &&
        bytes_at(0x304D30,(const unsigned char[]){0x40,0x53,0x48,0x83,0xEC,0x40,0x48,0x8B,1,0x48,0x8B,0xD9,0x48,0x8D,0x4C,0x24,0x20,0x48,0x89,0x44},20);
}

/* Original notification renderer has sixteen Win64 arguments. Private text
 * pointer identity selects our popup; all other calls preserve every argument. */
typedef void (__fastcall *RenderProc)(int,void *,void *,void *,void *,void *,void *,
    int,int,int,float,const unsigned char *,const unsigned char *,const unsigned char *,
    const int32_t *,const int32_t *);
typedef void *(__fastcall *TextProc)(void *,int,int,uint32_t,int,int,const unsigned char *,int);
typedef void *(__fastcall *BodyTextProc)(void *,int,uint32_t,int,int,const unsigned char *);
typedef void (__fastcall *ReleaseProc)(void *);
static int render_installed;
static void __fastcall render_popup(int init,void *frame,void *line,void *header,
    void *bottom,void *header_shape,void *footer_shape,int x,int y,int lines,float opacity,
    const unsigned char *label,const unsigned char *line1,const unsigned char *line2,
    const int32_t *boxcolor,const int32_t *textcolor) {
    if (label!=title || line1!=body) {
        ((RenderProc)(game+0x272930))(init,frame,line,header,bottom,header_shape,
            footer_shape,x,y,lines,opacity,label,line1,line2,boxcolor,textcolor);
        return;
    }
    NextLevelText runs[4]; next_level_layout(runs,label,line1,opacity,*(int32_t *)(game+0x232DCC8));
    for (unsigned i=0;i<4;++i) {
        if (!(runs[i].color>>24)) continue;
        void *packet=(void *)pointer(game+0x2846200);
        /* Use the original title face/size and the notification body path.
         * Body renderer selects native font size and baseline internally. */
        void *next=i<2 ? ((TextProc)(game+0x2D2840))(packet,1,0,runs[i].color,
            runs[i].x,runs[i].y,runs[i].text,runs[i].size) :
            ((BodyTextProc)(game+0x2D1CD0))(packet,3,runs[i].color,
            runs[i].x,runs[i].y,runs[i].text);
        *(void **)(game+0x2846200)=next;
        ((ReleaseProc)(game+0x304D30))(packet);
    }
}

static int install_render(void) {
    if (render_installed) return 1;
    uintptr_t site=game+0x27311A;
    /* A five-byte relative call needs a relay within two GiB. Allocate at
     * Windows allocation-granularity boundaries, then seal the relay RX. */
    unsigned char *relay=NULL;
    uintptr_t center=site & ~(uintptr_t)0xFFFF;
    for (uintptr_t delta=0x10000;delta<0x70000000 && !relay;delta+=0x10000) {
        if (center>=delta) relay=VirtualAlloc((void *)(center-delta),0x1000,
            MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if (!relay && center+delta>center) relay=VirtualAlloc((void *)(center+delta),
            0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    }
    if (!relay) return 0;
    relay[0]=0x48; relay[1]=0xB8;
    uintptr_t target=(uintptr_t)render_popup; memcpy(relay+2,&target,8);
    relay[10]=0xFF; relay[11]=0xE0;
    DWORD old,unused;
    if (!VirtualProtect(relay,0x1000,PAGE_EXECUTE_READ,&old)) {
        VirtualFree(relay,0,MEM_RELEASE); return 0;
    }
    FlushInstructionCache(GetCurrentProcess(),relay,12);
    unsigned char patch[5]={0xE8};
    int32_t displacement=(int32_t)((uintptr_t)relay-(site+5));
    memcpy(patch+1,&displacement,4);
    /* Collect handles before suspension: no allocations while game threads
     * are stopped. Never patch beneath a thread positioned inside the call. */
    HANDLE handles[256]; unsigned count=0,suspended=0; int safe=1,installed=0;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if (snapshot==INVALID_HANDLE_VALUE) { VirtualFree(relay,0,MEM_RELEASE); return 0; }
    THREADENTRY32 e={0}; e.dwSize=sizeof(e);
    if (!Thread32First(snapshot,&e)) safe=0;
    else do {
        if (e.th32OwnerProcessID!=GetCurrentProcessId() || e.th32ThreadID==GetCurrentThreadId()) continue;
        if (count==256) { safe=0; break; }
        HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,
            FALSE,e.th32ThreadID);
        if (!h) { safe=0; break; }
        handles[count++]=h;
    } while (Thread32Next(snapshot,&e));
    CloseHandle(snapshot);
    for (unsigned i=0;safe && i<count;++i) {
        if (SuspendThread(handles[i])==(DWORD)-1) { safe=0; break; }
        ++suspended;
        CONTEXT context={0}; context.ContextFlags=CONTEXT_CONTROL;
        if (!GetThreadContext(handles[i],&context) ||
            (context.Rip>=site && context.Rip<site+5)) safe=0;
    }
    if (safe && bytes_at(0x27311A,(const unsigned char[]){0xE8,0x11,0xF8,0xFF,0xFF},5) &&
        VirtualProtect((void *)site,5,PAGE_EXECUTE_READWRITE,&old)) {
        memcpy((void *)site,patch,5);
        FlushInstructionCache(GetCurrentProcess(),(void *)site,5);
        VirtualProtect((void *)site,5,old,&unused);
        installed=1;
    }
    for (unsigned i=0;i<suspended;++i) ResumeThread(handles[i]);
    for (unsigned i=0;i<count;++i) CloseHandle(handles[i]);
    if (!installed) VirtualFree(relay,0,MEM_RELEASE);
    render_installed=installed; return installed;
}

static void encode(unsigned char *out, const char *text) {
    memset(out,0,32);
    for (unsigned i=0; text[i] && i<31; ++i) {
        unsigned char c=(unsigned char)text[i];
        out[i]=(c>='A' && c<='Z') ? c-0x16 :
               (c>='0' && c<='9') ? c-0x0F : 1;
    }
}

static int owned(void) {
    return owned_box && readable(owned_box,0xBA0) &&
        pointer(owned_box+0x20)==(uintptr_t)body &&
        pointer(owned_box+0x30)==(uintptr_t)title;
}
static void hide(void) {
    /* Touch only a box still bearing both of this helper's private pointers. */
    if (owned()) *(uint32_t *)owned_box=0;
    owned_box=0; nextlevel_visible=0;
}

static int party_slot(uintptr_t save, uintptr_t actor) {
    uint32_t model_handle=u32(actor+0x130);
    uintptr_t model=(uintptr_t)((ResolveProc)(game+0x38ADC0))(model_handle);
    if (!readable(model,0x50)) return -1;
    unsigned character=*(uint16_t *)(model+0x4C);
    for (unsigned i=0;i<3;++i)
        if (*(unsigned char *)(save + 0x48E + i)==character) return (int)i;
    return -1;
}

static int show(uintptr_t save, uintptr_t actor, unsigned level, uint32_t remaining) {
    uintptr_t boxes=game+0x283B390;
    int slot=party_slot(save,actor);
    if (slot<0) return 0;
    /* Leave all vanilla level-up notices and other mods' notifications intact.
     * Only the first native party slot occupied by Sora is relevant here. */
    unsigned count=u32(game+0x283B380+(unsigned)slot*4);
    if (count>5) return 0;
    for (unsigned q=0;q<count;++q) {
        uintptr_t b=boxes+(unsigned)slot*0x3A20+q*0xBA0;
        if (u32(b) && (!owned() || b!=owned_box)) { hide(); return 0; }
    }
    char number[32];
    if (level==100) strcpy(number,"MAX LEVEL");
    else snprintf(number,sizeof(number),"%u",remaining);
    /* A new body requires native text resources to be reinitialized. Do not
     * mutate text under an already initialized animation. Cancel only our box
     * and enqueue a fresh one; one combined observation per game frame. */
    hide();
    encode(title,"NEXT LEVEL"); encode(body,number);
    ((EnqueueProc)(game+0x272540))((void *)actor,body,NULL);
    count=u32(game+0x283B380+(unsigned)slot*4);
    if (count>5) return 0;
    for (unsigned q=0;q<count;++q) {
        uintptr_t b=boxes+(unsigned)slot*0x3A20+q*0xBA0;
        if (pointer(b+0x20)==(uintptr_t)body && u32(b)==1) {
            *(uintptr_t *)(b+0x30)=(uintptr_t)title;
            *(uintptr_t *)(b+0xB88)=(uintptr_t)purple_box;
            *(uintptr_t *)(b+0xB90)=(uintptr_t)purple_text;
            owned_box=b; nextlevel_visible=1; ++nextlevel_popups;
            return 1;
        }
    }
    return 0;
}

static void tick(uint64_t now) {
    ++nextlevel_frames;
    NextLevelSample s={0};
    s.save=pointer(game+0x2868BA0); s.actor=pointer(game+0x2537E48);
    int visible=now-(uint64_t)InterlockedCompareExchange64(&lease,0,0)<2000 &&
        readable(s.save,0x492) && readable(s.actor,0x378) &&
        (u32(s.actor+0x374)&3)==1 && u32(s.actor+0x130) &&
        *(unsigned char *)(game+0x2D5CC4C)>0 &&
        *(float *)(game+0x281249C)>0.5f &&
        !*(unsigned char *)(game+0x22EC0AC) &&
        !*(unsigned char *)(game+0x23AB2D0) &&
        !*(unsigned char *)(game+0x233E808) &&
        !u32(game+0x5075A8) && !*(unsigned char *)(game+0x232DFC0) &&
        !*(unsigned char *)(game+0x2E20564);
    DWORD foreground=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    int f6=foreground==GetCurrentProcessId() && (GetAsyncKeyState(VK_F6)&0x8000)!=0;
    int preview=f6 && !was_f6; was_f6=f6;
    if (visible) {
        s.world=u32(game+0x233FE94); s.room=u32(game+0x233FE8C);
        s.scene=u32(game+0x233FE90); s.exp=u32(s.save+0x40);
        s.level=*(unsigned char *)(s.save+4); s.pace=*(unsigned char *)(s.save+0x48D);
        visible=s.level>=1 && s.level<=100 && s.pace<=2 && s.exp<=999999;
    }
    next_level_observe(&counter,&s,visible,preview,now);
    if (!visible) { hide(); return; }
    nextlevel_exp=s.exp; nextlevel_level=s.level; nextlevel_pace=s.pace;
    if (owned() && !u32(owned_box)) { owned_box=0; nextlevel_visible=0; }
    if (!owned()) { owned_box=0; nextlevel_visible=0; }
    /* Same resolver used by native EXP-award routine: battle-table base plus
     * the selected leveling-pace offset. Read current table, never hardcode XP. */
    uint32_t offset=u32(game+0x528028+s.pace*4), remaining=0;
    if (offset>0x11640-200 || (offset&1)) return;
    const uint16_t *costs=(const uint16_t *)(game+0x2D22D40+offset);
    if (!next_level_remaining(costs,s.level,s.exp,&remaining)) return;
    nextlevel_remaining=remaining;
    if (counter.pending) {
        if (show(s.save,s.actor,s.level,remaining)) counter.pending=0;
        else ++nextlevel_deferred;
    }
}

static uint64_t __cdecl on_frame(void *context) {
    /* Native gameplay and EXP changes complete before sampling. */
    uint64_t result=original_frame(context);
    tick(GetTickCount64());
    return result;
}

__declspec(dllexport) int __cdecl kh1_next_level_bootstrap(void *lua_state) {
    (void)lua_state;
    InterlockedExchange64(&lease,(LONG64)GetTickCount64());
    if (enabled || failed) return 0;
    game=(uintptr_t)GetModuleHandleW(L"KINGDOM HEARTS FINAL MIX.exe");
    if (!render_installed && !supported()) {
        failed=1; nextlevel_status=2; report("Unsupported executable or HUD function; disabled.");
        return 0;
    }
    uintptr_t app=pointer(game+0x21AAF18);
    if (!readable(app,8) || !readable(pointer(app),40)) return 0;
    FrameProc *slot=(FrameProc *)(pointer(app)+32);
    FrameProc prior=*slot;
    if (!prior || prior==on_frame) return 0;
    MEMORY_BASIC_INFORMATION m;
    if (!VirtualQuery((void *)(uintptr_t)prior,&m,sizeof(m)) ||
        !(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) {
        failed=1; nextlevel_status=2; report("Invalid frame callback; disabled."); return 0;
    }
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)on_frame,&pinned)) return 0;
    if (!install_render()) return 0;
    DWORD old,unused;
    if (!VirtualProtect(slot,8,PAGE_READWRITE,&old)) return 0;
    original_frame=prior;
    FrameProc observed=(FrameProc)InterlockedCompareExchangePointer(
        (PVOID volatile *)slot,(PVOID)(uintptr_t)on_frame,(PVOID)(uintptr_t)prior);
    VirtualProtect(slot,8,old,&unused);
    if (observed!=prior) return 0;
    enabled=1; nextlevel_status=1;
    report("Native HUD ready. Gain EXP or press F6. Gameplay and saves remain unchanged.");
    return 0;
}
