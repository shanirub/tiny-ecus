#include "tiny_cmd.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static bool is_space(char c)
{
    return c == ' ' || c == '\t';
}

static char to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

bool tiny_cmd_equals(const char *a, const char *b)
{
    while (*a && *b) {
        if (to_upper(*a) != to_upper(*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

// Split `line` in place into words. Returns the word count, or -1 if there are
// more than `max` words.
static int tokenize(char *line, char *argv[], int max)
{
    int argc = 0;
    char *p = line;
    for (;;) {
        while (is_space(*p)) {
            *p++ = '\0';
        }
        if (*p == '\0') {
            return argc;
        }
        if (argc == max) {
            return -1;
        }
        argv[argc++] = p;
        while (*p && !is_space(*p)) {
            p++;
        }
    }
}

static tiny_cmd_status_t reply_with(char *reply, size_t size, tiny_cmd_status_t st,
                                    const char *fmt, ...)
{
    int n = snprintf(reply, size, "%s", st == TINY_CMD_OK ? "OK" : "ERR");
    if (fmt != NULL && n >= 0 && (size_t)n < size) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(reply + n, size - (size_t)n, fmt, ap);
        va_end(ap);
    }
    return st;
}

static const tiny_cmd_t *find(const tiny_cmd_table_t *table, const char *name)
{
    for (size_t i = 0; i < table->count; i++) {
        if (tiny_cmd_equals(table->cmds[i].name, name)) {
            return &table->cmds[i];
        }
    }
    return NULL;
}

static tiny_cmd_status_t help(const tiny_cmd_table_t *table, int argc, char *argv[],
                              char *reply, size_t size)
{
    if (argc > 2) {
        return reply_with(reply, size, TINY_CMD_ERR, " usage: HELP [command]");
    }
    if (argc == 2) {
        const tiny_cmd_t *cmd = find(table, argv[1]);
        if (cmd == NULL) {
            return reply_with(reply, size, TINY_CMD_ERR, " unknown command '%s'", argv[1]);
        }
        return reply_with(reply, size, TINY_CMD_OK, " %s: %s", cmd->usage, cmd->help);
    }
    // List: "OK HELP STATUS TELEM ...", truncated by snprintf if it ever gets long.
    size_t n = (size_t)snprintf(reply, size, "OK HELP");
    for (size_t i = 0; i < table->count && n < size; i++) {
        n += (size_t)snprintf(reply + n, size - n, " %s", table->cmds[i].name);
    }
    return TINY_CMD_OK;
}

tiny_cmd_status_t tiny_cmd_execute(const tiny_cmd_table_t *table, char *line,
                                   char *reply, size_t reply_size)
{
    if (reply_size == 0) {
        return TINY_CMD_ERR;
    }
    reply[0] = '\0';

    char *argv[TINY_CMD_MAX_TOKENS];
    int argc = tokenize(line, argv, TINY_CMD_MAX_TOKENS);
    if (argc == 0) {
        return TINY_CMD_EMPTY;
    }
    if (argc < 0) {
        return reply_with(reply, reply_size, TINY_CMD_ERR, " too many words");
    }

    if (tiny_cmd_equals(argv[0], "HELP")) {
        return help(table, argc, argv, reply, reply_size);
    }

    const tiny_cmd_t *cmd = find(table, argv[0]);
    if (cmd == NULL) {
        return reply_with(reply, reply_size, TINY_CMD_ERR, " unknown command '%s'", argv[0]);
    }
    int nargs = argc - 1;
    if (nargs < cmd->min_args || nargs > cmd->max_args) {
        return reply_with(reply, reply_size, TINY_CMD_ERR, " usage: %s", cmd->usage);
    }

    // The handler writes its detail text after "OK " / "ERR ", decided by its result.
    char detail[TINY_CMD_REPLY_MAX];
    detail[0] = '\0';
    bool ok = cmd->handler(argc, argv, detail, sizeof detail);
    tiny_cmd_status_t st = ok ? TINY_CMD_OK : TINY_CMD_ERR;
    if (detail[0] == '\0') {
        return reply_with(reply, reply_size, st, NULL);
    }
    return reply_with(reply, reply_size, st, " %s", detail);
}

bool tiny_cmd_parse_long(const char *s, long min, long max, long *out)
{
    if (s == NULL || *s == '\0') {
        return false;
    }
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || v < min || v > max) {
        return false;
    }
    *out = v;
    return true;
}

bool tiny_cmd_parse_float(const char *s, float min, float max, float *out)
{
    if (s == NULL || *s == '\0') {
        return false;
    }
    char *end;
    errno = 0;
    float v = strtof(s, &end);
    if (errno != 0 || *end != '\0' || !isfinite(v) || v < min || v > max) {
        return false;
    }
    *out = v;
    return true;
}
