#pragma once

// Text command parser and line assembler. Plain C with no ESP-IDF includes,
// so it builds both in firmware and in the host unit tests (test/host).

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TINY_CMD_MAX_TOKENS  8    // command word + up to 7 arguments
#define TINY_CMD_REPLY_MAX   128  // reply buffer size used by callers, incl. NUL
#define TINY_LINE_MAX        96   // longest accepted input line, excl. NUL

/**
 * Command handler. argv[0] is the command word as typed, argv[1..argc-1] the
 * arguments; the argument count is already checked against the table entry.
 * Write optional detail text into `reply` (it becomes "OK <detail>" or
 * "ERR <detail>") and return true for success, false for failure.
 */
typedef bool (*tiny_cmd_handler_t)(int argc, char *argv[], char *reply, size_t reply_size);

typedef struct {
    const char *name;        // matched case-insensitively
    const char *usage;       // e.g. "TELEM ON|OFF", shown on wrong argument count
    const char *help;        // one line, shown by "HELP <name>"
    unsigned char min_args;  // not counting the command word
    unsigned char max_args;
    tiny_cmd_handler_t handler;
} tiny_cmd_t;

typedef struct {
    const tiny_cmd_t *cmds;
    size_t count;
} tiny_cmd_table_t;

typedef enum {
    TINY_CMD_OK,     // reply starts with "OK"
    TINY_CMD_ERR,    // reply starts with "ERR"
    TINY_CMD_EMPTY,  // blank line: no reply at all
} tiny_cmd_status_t;

/**
 * Parse and run one line. The line is modified in place (split into words).
 * Exactly one reply line, without a trailing newline, is written to `reply`,
 * except for a blank line, which leaves `reply` empty.
 *
 * "HELP" is built in: "HELP" lists all commands, "HELP <name>" gives the
 * usage and help text of one command.
 */
tiny_cmd_status_t tiny_cmd_execute(const tiny_cmd_table_t *table, char *line,
                                   char *reply, size_t reply_size);

/** Case-insensitive string equality (ASCII), e.g. for ON/OFF arguments. */
bool tiny_cmd_equals(const char *a, const char *b);

/**
 * Parse a whole decimal integer in [min, max]. Rejects empty strings, trailing
 * characters ("60abc"), and out-of-range values. `out` is untouched on failure.
 */
bool tiny_cmd_parse_long(const char *s, long min, long max, long *out);

/** As tiny_cmd_parse_long, for a decimal number; also rejects NaN and infinity. */
bool tiny_cmd_parse_float(const char *s, float min, float max, float *out);

/**
 * Line assembler: collects bytes from a serial stream into lines.
 * CR, LF and CRLF all end a line. Backspace (0x08 or 0x7F) deletes the last
 * character. Other control characters except tab are dropped.
 */
typedef struct {
    char buf[TINY_LINE_MAX + 1];
    size_t len;
    bool overflow;  // current line is longer than TINY_LINE_MAX; discarding it
    bool last_cr;   // previous byte was CR, so a following LF is not a new line
    bool ready;     // buf held a finished line; start over on the next byte
} tiny_line_t;

typedef enum {
    TINY_LINE_NONE,      // keep feeding
    TINY_LINE_READY,     // buf holds a NUL-terminated line, valid until the next feed
    TINY_LINE_TOO_LONG,  // a line ended that was too long; it was discarded
} tiny_line_result_t;

void tiny_line_init(tiny_line_t *l);
tiny_line_result_t tiny_line_feed(tiny_line_t *l, char c);

#ifdef __cplusplus
}
#endif
