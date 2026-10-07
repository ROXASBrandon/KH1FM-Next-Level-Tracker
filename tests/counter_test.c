#include <assert.h>
#include <stdio.h>
#include "counter.h"

int main(void) {
    uint16_t costs[100];
    for (unsigned i=0;i<100;++i) costs[i]=(uint16_t)(10+i);
    uint32_t n=999;
    assert(next_level_remaining(costs,1,0,&n) && n==10);
    assert(next_level_remaining(costs,1,9,&n) && n==1);
    assert(!next_level_remaining(costs,1,10,&n)); /* in-flight level change */
    assert(next_level_remaining(costs,2,10,&n) && n==11);
    assert(next_level_remaining(costs,2,20,&n) && n==1);
    assert(next_level_remaining(costs,100,999999,&n) && n==0);
    assert(!next_level_remaining(costs,0,0,&n));
    assert(!next_level_remaining(costs,101,0,&n));
    assert(!next_level_remaining(costs,1,1000000,&n));
    costs[0]=0; assert(!next_level_remaining(costs,1,0,&n));
    assert(!next_level_remaining(NULL,1,0,&n));
    costs[0]=10;

    NextLevelCounter c={0};
    NextLevelSample s={.save=1,.actor=2,.world=3,.room=4,.scene=5,.exp=0,.level=1,.pace=0};
    next_level_observe(&c,&s,1,0,100); assert(!c.pending);
    s.exp=3;
    next_level_observe(&c,&s,1,0,200); assert(c.pending && c.deadline==4200);
    c.pending=0; /* successful HUD submission */
    next_level_observe(&c,&s,1,0,201); assert(!c.pending);
    s.exp=10; s.level=2;
    next_level_observe(&c,&s,1,0,300); assert(c.pending);
    next_level_observe(&c,&s,0,0,400); assert(!c.pending && !c.baseline);
    s.exp=200; s.level=10;
    next_level_observe(&c,&s,1,0,500); assert(!c.pending); /* loading save */
    s.exp=201;
    next_level_observe(&c,&s,1,0,600); assert(c.pending);
    s.room=9;
    next_level_observe(&c,&s,1,0,700); assert(!c.pending); /* no old popup after warp */
    s.exp=100; s.level=5;
    next_level_observe(&c,&s,1,0,800); assert(!c.pending); /* death/retry */
    next_level_observe(&c,&s,1,1,900); assert(c.pending); /* preview */
    next_level_observe(&c,&s,1,0,4900); assert(!c.pending); /* stale deferred popup */
    s.pace=2;
    next_level_observe(&c,&s,1,0,5000); assert(!c.pending);
    puts("PASS: EXP costs, level transitions, max level, load, warp, retry, preview, expiry.");
    return 0;
}
