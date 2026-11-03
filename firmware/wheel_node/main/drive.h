#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tiny_motor.h"

/**
 * Configure the motor driver pins (driver asleep, inputs low) and start the
 * 100 Hz drive loop, which also sends the telemetry rows. Call once at
 * startup, after encoder_start() and telemetry_start().
 */
void drive_start(void);

/**
 * Start a timed run at `duty_pct` percent (-100..100, capped to the duty cap),
 * for `seconds`, then ramp to a stop. Sets `*run_pct` to the duty it will run
 * at, after the cap. Returns false while a stall fault is latched. Takes effect
 * at the next control period (within 10 ms).
 */
bool drive_run(int32_t duty_pct, uint32_t seconds, int32_t *run_pct);

/**
 * Start a timed speed-controlled run at `rpm` wheel rpm (signed like the duty),
 * for `seconds`, then ramp to a stop. The PI controller sets the duty every
 * period; the duty cap, slew, stall trip and timeout still apply. Sent during
 * a SPEED run, it changes the setpoint and restarts the timer without
 * resetting the controller. Returns false while a stall fault is latched.
 */
bool drive_speed(int32_t rpm, uint32_t seconds);

/** Stop at once (coast, driver asleep) and clear a stall fault. */
void drive_stop(void);

/**
 * Select the PWM decay mode for the next runs (slow at boot). Refused (returns
 * false) while a run is going, so the pin mapping never changes mid-run.
 */
bool drive_set_decay(tiny_motor_decay_t decay);

typedef enum {
    DRIVE_GAIN_KP,  // permille duty per rpm of error
    DRIVE_GAIN_KI,  // permille duty per rpm of error per second
    DRIVE_GAIN_FF,  // feedforward, permille duty per rpm of setpoint
} drive_gain_t;

/** Change one controller gain; takes effect at the next period, also mid-run. */
void drive_set_gain(drive_gain_t gain, float value);

/** The current controller gains. */
void drive_gains(float *kp, float *ki, float *ff);

/** Current mode, output duty in percent and decay mode. */
void drive_status(tiny_motor_mode_t *mode, int32_t *duty_pct, tiny_motor_decay_t *decay);
