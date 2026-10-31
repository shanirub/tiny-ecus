#include "tiny_motor.h"

static int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static int32_t abs32(int32_t v)
{
    return v < 0 ? -v : v;
}

void tiny_motor_init(tiny_motor_t *m)
{
    tiny_motor_stop(m);
}

bool tiny_motor_run(tiny_motor_t *m, const tiny_motor_cfg_t *cfg, int32_t duty, uint32_t steps)
{
    if (m->mode == TINY_MOTOR_FAULT) {
        return false;
    }
    m->mode = TINY_MOTOR_RUN;
    // A run of 0 steps is already over: ramp down from wherever the duty is.
    m->target = steps > 0 ? clamp(duty, -cfg->cap, cfg->cap) : 0;
    m->steps_left = steps;
    return true;
}

void tiny_motor_stop(tiny_motor_t *m)
{
    m->mode = TINY_MOTOR_IDLE;
    m->target = 0;
    m->duty = 0;
    m->steps_left = 0;
    m->stall_steps = 0;
}

int32_t tiny_motor_step(tiny_motor_t *m, const tiny_motor_cfg_t *cfg, int32_t counts)
{
    if (m->mode != TINY_MOTOR_RUN) {
        return 0;
    }

    // Stall check on the period that just ended, which ran at m->duty. Range
    // test, not abs32(counts): abs32(INT32_MIN) would overflow.
    if (abs32(m->duty) >= cfg->stall_duty
        && counts >= -cfg->stall_counts && counts <= cfg->stall_counts) {
        if (++m->stall_steps >= cfg->stall_steps) {
            m->mode = TINY_MOTOR_FAULT;
            m->target = 0;
            m->duty = 0;
            m->steps_left = 0;
            return 0;
        }
    } else {
        m->stall_steps = 0;
    }

    if (m->steps_left > 0 && --m->steps_left == 0) {
        m->target = 0;  // run over: ramp down
    }
    m->duty += clamp(m->target - m->duty, -cfg->slew, cfg->slew);
    if (m->target == 0 && m->duty == 0) {
        m->mode = TINY_MOTOR_IDLE;
    }
    return m->duty;
}

void tiny_motor_pins(int32_t duty, uint32_t full, uint32_t *in1, uint32_t *in2)
{
    duty = clamp(duty, -TINY_MOTOR_DUTY_MAX, TINY_MOTOR_DUTY_MAX);
    // The pulsed pin is high (braking) for the part of the period not driving.
    uint32_t brake = full - (uint32_t)((uint64_t)full * (uint32_t)abs32(duty) / TINY_MOTOR_DUTY_MAX);
    *in1 = duty < 0 ? brake : full;
    *in2 = duty > 0 ? brake : full;
}
