#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Configure the encoder pins (ENC_A_GPIO, ENC_B_GPIO in pins.h) and start counting:
 * a GPIO interrupt on every edge of both pins feeds the quadrature decoder
 * (components/tiny_qdec). Call once at startup.
 */
void encoder_start(void);

/**
 * Read the position (net counts since start or the last zero; 4320 per wheel
 * turn expected) and the number of invalid transitions (missed edges), both
 * taken at the same instant.
 */
void encoder_read(int32_t *position, uint32_t *invalid);

/** Set position and invalid count to 0. */
void encoder_zero(void);

/**
 * Measurement probe on ISR_PROBE_GPIO (pins.h): when on, the pin is an output
 * that is high while the interrupt handler runs; when off (the default), the
 * pin is left undriven with no pulls, as after reset.
 */
void encoder_set_probe(bool on);
bool encoder_probe(void);
