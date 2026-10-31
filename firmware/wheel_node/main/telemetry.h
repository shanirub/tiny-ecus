#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tiny_motor.h"

// One telemetry row per 10 ms control period, sent by the drive task: what the
// motor output was during the period and what the encoder saw. Item E adds
// the setpoint.
typedef struct {
    uint32_t t_ms;               // end of the period
    tiny_motor_mode_t mode;      // 0 idle, 1 run, 2 stall fault
    int32_t duty_permille;       // signed duty output during the period
    int32_t counts;              // encoder counts during the period
    int32_t rpm_milli;           // wheel speed from those counts (tiny_speed)
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
