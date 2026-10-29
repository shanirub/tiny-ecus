#pragma once

// Wheel node pin map (ESP32-C3 Super Mini): every GPIO the firmware uses is
// defined here and nowhere else. The reasons behind each choice are in the
// Phase 1 plan's pin map (.planning/can-rc-car/phase-1-one-wheel-node.md,
// re-mapped 2026-10-22). Rule from there: driver inputs only on pins with no
// pull at reset; pins with a reset pull-up go to inputs.

#include "driver/gpio.h"

// Onboard status LED, active-low (verified 2026-10-17).
#define LED_GPIO          GPIO_NUM_8
#define LED_ACTIVE_LOW    1

// Encoder channels, interrupt inputs. Both have the C3's pull-up on at reset,
// harmless on inputs the encoder drives. GPIO20 is UART0 RX, free because the
// console is on USB.
#define ENC_A_GPIO        GPIO_NUM_6
#define ENC_B_GPIO        GPIO_NUM_20

// DRV8833 module (item D, not driven yet): no pull at reset, so the module's
// pull-downs keep the motor off until the firmware drives them.
#define MOTOR_IN1_GPIO    GPIO_NUM_5   // module AIN1 (PWM)
#define MOTOR_IN2_GPIO    GPIO_NUM_7   // module AIN2 (PWM)
#define MOTOR_STBY_GPIO   GPIO_NUM_10  // module STBY: low = driver asleep

// Reserved, not defined until their code exists:
//   GPIO3 / 4  CAN TX / RX (Phase 2)
//   GPIO0 / 1  battery ADC (Phase 5) / corner LED (Phase 4)
