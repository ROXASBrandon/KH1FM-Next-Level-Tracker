#include <assert.h>
#include <math.h>
#include "hud.h"
int main(void) {
    const unsigned char label[]={1,0}, number[]={2,0};
    NextLevelText t[4];
    next_level_layout(t,label,number,1,-70);
    assert(t[1].x==-54 && t[1].y==66 && t[3].y==82);
    assert(t[1].text==label && t[3].text==number);
    assert(t[1].x==t[3].x && t[1].y>60 && t[3].y>=t[1].y+t[1].size);
    assert(t[1].size==12 && t[3].size==18);
    assert(t[0].x==t[1].x+1 && t[0].y==t[1].y+1);
    assert(t[2].x==t[3].x+1 && t[2].y==t[3].y+1);
    assert((t[1].color>>24)==128 && (t[0].color>>24)==96);
    next_level_layout(t,label,number,0.5f,-70); assert((t[3].color>>24)==64);
    next_level_layout(t,label,number,-1,-70); assert((t[3].color>>24)==0);
    next_level_layout(t,label,number,NAN,-70); assert((t[3].color>>24)==0);
    next_level_layout(t,label,number,2,-70); assert((t[3].color>>24)==128);
    next_level_layout(t,label,number,1,0); assert(t[1].x==16 && t[3].x==16);
    return 0;
}
