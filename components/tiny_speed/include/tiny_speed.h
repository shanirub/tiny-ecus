#pragma once

// Wheel speed from encoder counts. Plain C with no ESP-IDF includes, so it
// builds both in firmware and in the host unit tests (test/host).
//
// Phase 1 D.3 starts with the plainest estimate: the counts of one control
// period divided by its length. At 100 Hz and 4320 counts per wheel turn, one
// count per period is 1.389 rpm, so slow speeds read in coarse steps. A moving
// average (or timing the gap between edges) comes only if the data needs it.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speed in milli-rpm (rpm x 1000) for `counts` encoder counts over
 * `period_ms` milliseconds, with `counts_per_turn` counts per wheel turn.
 * Signed like the counts. Rounded to the nearest milli-rpm, halves away from
 * zero, and clamped to the int32 range. Returns 0 if `period_ms` or
 * `counts_per_turn` is 0.
 */
int32_t tiny_speed_rpm_milli(int32_t counts, uint32_t period_ms, uint32_t counts_per_turn);

#ifdef __cplusplus
}
#endif
