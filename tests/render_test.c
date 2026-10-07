/* Windows-only synthetic renderer probe. No game process or save is accessed. */
#include <assert.h>
#include "../native/popup.c"
static int vanilla_calls,text_calls,release_calls;
static uintptr_t packet_value=0x1000;
static unsigned char other_label[1],other_body[1];
static void __fastcall mock_vanilla(int init,void *a,void *b,void *c,void *d,void *e,
    void *f,int x,int y,int lines,float fade,const unsigned char *label,
    const unsigned char *line1,const unsigned char *line2,const int32_t *box,const int32_t *text) {
    assert(init==7 && a==(void *)1 && b==(void *)2 && c==(void *)3);
    assert(d==(void *)4 && e==(void *)5 && f==(void *)6);
    assert(x==111 && y==222 && lines==2 && fade==0.5f);
    assert(label==other_label && line1==other_body && line2==(void *)9);
    assert(box==purple_box && text==purple_text); ++vanilla_calls;
}
static void *__fastcall mock_text(void *packet,int mode,int align,uint32_t color,
    int x,int y,const unsigned char *text,int size) {
    NextLevelText expected[4]; next_level_layout(expected,title,body,0.5f,-70);
    int i=text_calls++;
    assert(i<2 && packet==(void *)packet_value && mode==1 && align==0);
    assert(x==expected[i].x && y==expected[i].y && color==expected[i].color);
    assert(text==expected[i].text && size==expected[i].size);
    return (void *)++packet_value;
}
static void *__fastcall mock_body(void *packet,int mode,uint32_t color,
    int x,int y,const unsigned char *text) {
    NextLevelText expected[4]; next_level_layout(expected,title,body,0.5f,-70);
    int i=text_calls++;
    assert(i>=2 && i<4 && packet==(void *)packet_value && mode==3);
    assert(x==expected[i].x && y==expected[i].y && color==expected[i].color);
    assert(text==expected[i].text);
    return (void *)++packet_value;
}
static void __fastcall mock_release(void *packet) {
    assert(packet==(void *)(packet_value-1)); ++release_calls;
}
static void jump(uintptr_t address,uintptr_t target) {
    unsigned char *p=(unsigned char *)address;
    p[0]=0x48; p[1]=0xB8; memcpy(p+2,&target,8); p[10]=0xFF; p[11]=0xE0;
}
int main(void) {
    game=(uintptr_t)VirtualAlloc(NULL,0x2850000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(game);
    jump(game+0x272930,(uintptr_t)mock_vanilla);
    *(int32_t *)(game+0x232DCC8)=-70;
    jump(game+0x2D1CD0,(uintptr_t)mock_body);
    jump(game+0x2D2840,(uintptr_t)mock_text);
    jump(game+0x304D30,(uintptr_t)mock_release);
    FlushInstructionCache(GetCurrentProcess(),(void *)game,0x2850000);
    *(uintptr_t *)(game+0x2846200)=packet_value;
    render_popup(7,(void *)1,(void *)2,(void *)3,(void *)4,(void *)5,(void *)6,
        111,222,2,0.5f,other_label,other_body,(void *)9,purple_box,purple_text);
    assert(vanilla_calls==1 && text_calls==0 && release_calls==0);
    render_popup(0,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1,0.5f,title,body,NULL,NULL,NULL);
    assert(vanilla_calls==1 && text_calls==4 && release_calls==4);
    assert(pointer(game+0x2846200)==packet_value);
    render_popup(0,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1,0,title,body,NULL,NULL,NULL);
    assert(text_calls==4 && release_calls==4);
    VirtualFree((void *)game,0,MEM_RELEASE);
    puts("PASS: Win64 renderer arguments, vanilla passthrough, private text, packet lifecycle and fade.");
    return 0;
}
