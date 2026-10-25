#pragma once

#include <stdint.h>

/**
 * Configure the encoder pins (A = GPIO6, B = GPIO20) and start counting:
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
