#ifndef NEXT_LEVEL_COUNTER_H
#define NEXT_LEVEL_COUNTER_H
#include <stdint.h>
#include <stddef.h>

/* The executable's EXP-award loop (Steam 1.0.0.2, 0x284940) subtracts
 * successive uint16_t costs from total EXP. Costs are not cumulative. */
static int next_level_remaining(const uint16_t *costs, unsigned level,
                                uint32_t exp, uint32_t *remaining) {
    if (!costs || !remaining || level < 1 || level > 100 || exp > 999999)
        return 0;
    if (level == 100) { *remaining = 0; return 1; }
    uint32_t threshold = 0;
    for (unsigned i = 0; i < level; ++i) {
        if (!costs[i]) return 0;
        threshold += costs[i];
    }
    /* Wait for the native level update instead of briefly reporting zero. */
    if (exp >= threshold) return 0;
    *remaining = threshold - exp;
    return 1;
}

typedef struct {
    uintptr_t save, actor;
    uint32_t world, room, scene, exp;
    unsigned level, pace;
} NextLevelSample;

typedef struct {
    NextLevelSample last;
    uint64_t deadline;
    int baseline, pending;
} NextLevelCounter;

static int next_level_same_context(const NextLevelSample *a, const NextLevelSample *b) {
    return a->save == b->save && a->actor == b->actor && a->world == b->world &&
           a->room == b->room && a->scene == b->scene && a->pace == b->pace;
}

/* Read-only observation. EXP losses/retries, warps, load and hidden gameplay
 * reset the baseline; no old gain is replayed after returning to gameplay. */
static void next_level_observe(NextLevelCounter *c, const NextLevelSample *s,
                               int visible, int preview, uint64_t now) {
    if (!visible) { c->baseline = c->pending = 0; return; }
    if (!c->baseline || !next_level_same_context(&c->last, s) ||
        s->exp < c->last.exp || s->level < c->last.level) {
        c->last = *s; c->baseline = 1; c->pending = 0;
    } else {
        if (s->exp > c->last.exp || s->level > c->last.level) {
            c->pending = 1; c->deadline = now + 4000;
        }
        c->last = *s;
    }
    if (preview) { c->pending = 1; c->deadline = now + 4000; }
    if (c->pending && now >= c->deadline) c->pending = 0;
}
#endif
