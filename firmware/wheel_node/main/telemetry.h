#pragma once

#include <stdbool.h>
#include <stdint.h>

// One telemetry row. Phase 1 B carries a synthetic test signal; items C–E
// replace it with real columns (position, speed, duty, setpoint).
typedef struct {
    uint32_t t_ms;
    int32_t sine_milli;  // test signal, amplitude 1000
} telemetry_sample_t;

/** Create the sample queue and the low-priority writer task. */
void telemetry_start(void);

/**
 * Queue one sample without blocking; drops it (and counts the drop) when the
 * queue is full or telemetry is off. Safe to call from the control loop.
 */
void telemetry_push(const telemetry_sample_t *s);

void telemetry_set_enabled(bool on);
bool telemetry_enabled(void);
uint32_t telemetry_dropped(void);
