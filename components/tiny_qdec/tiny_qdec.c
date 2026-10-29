#include "tiny_qdec.h"

#define INV TINY_QDEC_INVALID

// Index: (prev << 2) | now, states written AB. One row per previous state.
// Forward ring (+1): 10 -> 11 -> 01 -> 00 -> 10. Diagonal: no change (0).
// Both bits changed: INV.
static const int8_t s_step[16] = {
    //  now: 00   01   10   11
             0,  -1,  +1,  INV,  // prev 00
            +1,   0,  INV, -1,   // prev 01
            -1,  INV,  0,  +1,   // prev 10
            INV, +1,  -1,   0,   // prev 11
};

int tiny_qdec_step(uint8_t prev, uint8_t now)
{
    return s_step[((prev & 3u) << 2) | (now & 3u)];
}

void tiny_qdec_init(tiny_qdec_t *q, uint8_t state)
{
    q->position = 0;
    q->invalid = 0;
    q->state = state & 3u;
}

void tiny_qdec_update(tiny_qdec_t *q, uint8_t now)
{
    now &= 3u;
    int step = tiny_qdec_step(q->state, now);
    if (step == INV) {
        q->invalid++;
    } else {
        // Unsigned add, then convert back: wraps at 2^32 instead of signed
        // overflow, which is undefined in C. Speed is taken from differences,
        // which stay correct across the wrap.
        q->position = (int32_t)((uint32_t)q->position + (uint32_t)step);
    }
    q->state = now;
}

void tiny_qdec_zero(tiny_qdec_t *q)
{
    q->position = 0;
    q->invalid = 0;
}
