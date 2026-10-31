#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tiny_motor.h"

/**
 * Configure the motor driver pins (driver asleep, inputs low) and start the
 * 100 Hz drive loop. Call once at startup, after encoder_start().
 */
void drive_start(void);

/**
 * Start a timed run at `duty_pct` percent (-100..100, capped to the duty cap),
 * for `seconds`, then ramp to a stop. Sets `*run_pct` to the duty it will run
 * at, after the cap. Returns false while a stall fault is latched. Takes effect
 * at the next control period (within 10 ms).
 */
bool drive_run(int32_t duty_pct, uint32_t seconds, int32_t *run_pct);

/** Stop at once (coast, driver asleep) and clear a stall fault. */
void drive_stop(void);

/** Current mode and output duty in percent. */
void drive_status(tiny_motor_mode_t *mode, int32_t *duty_pct);
