#pragma once

// Quadrature decoder: turns the encoder's A/B levels into a signed position.
// Plain C with no ESP-IDF includes, so it builds both in firmware and in the
// host unit tests (test/host). In firmware, linker.lf places it in IRAM/DRAM
// so the encoder ISR can call it while the flash is busy.
//
// A state packs the two levels as (A << 1) | B, written "AB" below. In the
// direction recorded in Phase 0 F.4, A leads B and the states run
// 10 -> 11 -> 01 -> 00 -> 10; each of those steps counts +1. The opposite
// order counts -1. Which direction is "forward" is decided later (Phase 1 D.4).

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TINY_QDEC_INVALID  2  // tiny_qdec_step(): both levels changed at once

typedef struct {
    int32_t position;  // net steps since init or zero; wraps modulo 2^32
    uint32_t invalid;  // transitions where both A and B changed (a missed edge)
    uint8_t state;     // last seen AB state, 0..3
} tiny_qdec_t;

/** Pack the two levels into a state 0..3. */
static inline uint8_t tiny_qdec_state(bool a, bool b)
{
    return (uint8_t)((a ? 2u : 0u) | (b ? 1u : 0u));
}

/**
 * The step from state `prev` to state `now` (both 0..3): +1, -1, 0 (no
 * change) or TINY_QDEC_INVALID.
 */
int tiny_qdec_step(uint8_t prev, uint8_t now);

/** Start at position 0 with no invalid transitions, from the current state. */
void tiny_qdec_init(tiny_qdec_t *q, uint8_t state);

/**
 * Feed a newly read state. A valid step moves the position by +-1; an invalid
 * one only increments `invalid`. Either way `now` becomes the reference for
 * the next call, so the decoder resynchronises after a missed edge.
 */
void tiny_qdec_update(tiny_qdec_t *q, uint8_t now);

/** Clear position and invalid count; keep the current state. */
void tiny_qdec_zero(tiny_qdec_t *q);

#ifdef __cplusplus
}
#endif
