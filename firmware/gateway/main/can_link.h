#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Start the CAN node and the task that reads received frames: it notes when
 * the wheel node's status frame last arrived and, with the monitor on, prints
 * every frame as a "C," line.
 */
void can_link_start(void);

/** Milliseconds since the last status frame; false if none has arrived yet. */
bool can_link_status_age(uint32_t *age_ms);

/**
 * Print each received frame as one line:
 *   C,<ms>,<id>,<len>,<data hex>
 * <ms> is the gateway's uptime when the frame was taken from the queue, <id>
 * three hex digits, <data hex> two hex digits per byte (empty for len 0).
 */
void can_link_set_monitor(bool on);
bool can_link_monitor(void);
