#include "tiny_pi.h"

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

void tiny_pi_reset(tiny_pi_t *pi)
{
    pi->integral = 0.0f;
}

float tiny_pi_feedforward(const tiny_pi_cfg_t *cfg, float setpoint)
{
    if (setpoint > 0.0f) {
        return cfg->ff * (setpoint + cfg->ff_offset);
    }
    if (setpoint < 0.0f) {
        return cfg->ff * (setpoint - cfg->ff_offset);
    }
    return 0.0f;
}

int32_t tiny_pi_step(tiny_pi_t *pi, const tiny_pi_cfg_t *cfg, float setpoint, float measured)
{
    float error = setpoint - measured;
    float base = tiny_pi_feedforward(cfg, setpoint) + cfg->kp * error;
    float integral = clampf(pi->integral + cfg->ki * error * cfg->dt, -cfg->out_max, cfg->out_max);
    float out = base + integral;

    // Anti-windup by conditional integration: keep the new integral unless the
    // output is past its limit and the error would push it further out.
    if ((out > cfg->out_max && error > 0.0f) || (out < -cfg->out_max && error < 0.0f)) {
        out = base + pi->integral;
    } else {
        pi->integral = integral;
    }

    out = clampf(out, -cfg->out_max, cfg->out_max);
    return (int32_t)(out >= 0.0f ? out + 0.5f : out - 0.5f);
}
