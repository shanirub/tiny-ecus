#pragma once

// Motor run logic for one wheel: timed runs, duty cap, slew limit (soft
// start), stall detection, and the mapping from a signed duty to the two
// DRV8833 input duties. Plain C with no ESP-IDF includes, so it builds both in
// firmware and in the host unit tests (test/host). The firmware calls
// tiny_motor_step() once per control period and applies its result.
//
// Duty is signed, in permille: +1000 is full forward, -1000 full reverse.
// On the bench motor, positive duty turns the wheel clockwise seen from its
// outer side (Phase 1 D.4.1), and the encoder counts up (sign set in
// encoder.c, Phase 1 E).

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TINY_MOTOR_DUTY_MAX  1000  // permille

typedef struct {
    int32_t cap;             // |duty| never exceeds this, permille
    int32_t slew;            // max duty change per step, permille
    int32_t stall_duty;      // stall check applies at |duty| >= this, permille
    int32_t stall_counts;    // |encoder counts| per step at or below this = not moving
    uint32_t stall_steps;    // that many steps in a row = stalled
} tiny_motor_cfg_t;

// What the motor does in the off part of each PWM period (DRV8833 datasheet,
// "PWM control"). Slow decay shorts the motor through the driver: the current
// fades slowly and brakes a little. Fast decay lets both pins go low: the
// current falls quickly back into the supply and the motor coasts.
typedef enum {
    TINY_MOTOR_DECAY_SLOW,
    TINY_MOTOR_DECAY_FAST,
} tiny_motor_decay_t;

typedef enum {
    TINY_MOTOR_IDLE,   // stopped, output off
    TINY_MOTOR_RUN,    // running or ramping down at the end of a run
    TINY_MOTOR_FAULT,  // stopped by the stall detector; only tiny_motor_stop() clears it
} tiny_motor_mode_t;

typedef struct {
    tiny_motor_mode_t mode;
    int32_t target;        // duty the run asks for (already capped), permille
    int32_t duty;          // duty being output, moves towards target by at most slew per step
    uint32_t steps_left;   // steps until the run ends and the duty ramps to 0
    uint32_t stall_steps;  // consecutive steps that looked stalled
} tiny_motor_t;

void tiny_motor_init(tiny_motor_t *m);

/**
 * Start (or change) a timed run: ramp to `duty` (capped to cfg->cap) and hold
 * it until `steps` steps from now (the ramp up counts toward them), then ramp
 * to 0 and go idle. Refused (returns false, nothing changes) while in FAULT.
 */
bool tiny_motor_run(tiny_motor_t *m, const tiny_motor_cfg_t *cfg, int32_t duty, uint32_t steps);

/**
 * Change the target duty of the run in progress without restarting its timer,
 * e.g. from a speed controller every step. Capped to cfg->cap; the output still
 * moves there by at most cfg->slew per step. Ignored unless running with time
 * left, so it can't restart a run that has ended or is ramping down.
 */
void tiny_motor_set_target(tiny_motor_t *m, const tiny_motor_cfg_t *cfg, int32_t duty);

/** Stop at once (duty 0, IDLE, no ramp) and clear a FAULT. */
void tiny_motor_stop(tiny_motor_t *m);

/**
 * One control period. `counts` is the encoder's position change during the
 * period that just ended, which ran at the duty from the previous step.
 * Returns the duty to output for the next period. In IDLE and FAULT it is 0
 * and the caller should switch the driver off.
 */
int32_t tiny_motor_step(tiny_motor_t *m, const tiny_motor_cfg_t *cfg, int32_t counts);

/**
 * DRV8833 mapping from a signed duty to the duty of each input pin, in 0..full
 * (full = 2^resolution of the PWM timer, i.e. constantly high). Reverse mirrors
 * forward in both modes.
 *  - Slow decay: forward holds IN1 high and PWMs IN2: IN2 low drives forward,
 *    IN2 high brakes (both high). Duty 0 gives both high: brake.
 *  - Fast decay: forward PWMs IN1 and holds IN2 low: IN1 high drives forward,
 *    IN1 low coasts (both low). Duty 0 gives both low: coast.
 */
void tiny_motor_pins(int32_t duty, tiny_motor_decay_t decay, uint32_t full, uint32_t *in1, uint32_t *in2);

#ifdef __cplusplus
}
#endif
