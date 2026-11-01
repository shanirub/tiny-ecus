#pragma once

// PI speed controller with feedforward and anti-windup, for one wheel. Plain C
// with no ESP-IDF includes, so it builds both in firmware and in the host unit
// tests (test/host). The firmware calls tiny_pi_step() once per control period
// and hands the result to tiny_motor as the run's target duty, so the duty
// cap, slew limit, stall trip and run timeout still apply on top.
//
// Units: speeds in wheel rpm, output in duty permille (as tiny_motor). Positive
// duty turns the wheel the way the encoder counts up, so positive rpm.
//
//   output = feedforward(setpoint) + kp * error + integral
//   error  = setpoint - measured
//   integral += ki * error * dt, frozen while the output is saturated in the
//   direction the error pushes (anti-windup)
//
// The integral is stored in permille (already multiplied by ki), so changing
// ki while running doesn't make the output jump.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp;         // permille duty per rpm of error
    float ki;         // permille duty per rpm of error per second
    float ff;         // feedforward slope: permille duty per rpm of setpoint (0 = off)
    float ff_offset;  // rpm added to |setpoint| before the slope (friction to overcome)
    float dt;         // control period, s
    float out_max;    // |output| limit, permille (the duty cap)
} tiny_pi_cfg_t;

typedef struct {
    float integral;   // permille, within +-out_max
} tiny_pi_t;

/** Clear the integral. Call when a new speed run starts from rest. */
void tiny_pi_reset(tiny_pi_t *pi);

/**
 * The feedforward duty for `setpoint`, permille, not limited: 0 at setpoint 0,
 * otherwise sign(setpoint) * ff * (|setpoint| + ff_offset).
 */
float tiny_pi_feedforward(const tiny_pi_cfg_t *cfg, float setpoint);

/**
 * One control period: the duty, permille, rounded and limited to +-out_max,
 * for the wanted speed `setpoint` given the speed `measured` over the period
 * that just ended.
 */
int32_t tiny_pi_step(tiny_pi_t *pi, const tiny_pi_cfg_t *cfg, float setpoint, float measured);

#ifdef __cplusplus
}
#endif
