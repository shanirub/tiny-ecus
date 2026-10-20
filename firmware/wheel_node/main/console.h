#pragma once

/**
 * Install the USB-Serial-JTAG driver and start the console task, which reads
 * command lines from the USB port and answers each with one OK/ERR line.
 */
void console_start(void);
