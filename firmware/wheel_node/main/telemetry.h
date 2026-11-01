#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tiny_motor.h"

// The mode column: tiny_motor's modes, with a run split into open loop (DUTY)
// and closed loop (SPEED). 0-2 keep the meaning they had before item E.
typedef enum {
    TELEM_MODE_IDLE = 0,
    TELEM_MODE_DUTY = 1,   // open-loop run
    TELEM_MODE_STALL = 2,  // stall fault latched
    TELEM_MODE_SPEED = 3,  // speed-controlled run
} telem_mode_t;

// One telemetry row per 10 ms control period, sent by the drive task: what the
// motor output was during the period and what the encoder saw.
typedef struct {
    uint32_t t_ms;               // end of the period
    telem_mode_t mode;
    int32_t setpoint_milli;      // wanted speed in a SPEED run, rpm x 1000; else 0
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
