#include "tiny_cmd.h"

#include <string.h>

void tiny_line_init(tiny_line_t *l)
{
    memset(l, 0, sizeof *l);
}

tiny_line_result_t tiny_line_feed(tiny_line_t *l, char c)
{
    // Second half of CRLF: swallow it before anything else, so the line that
    // the CR finished stays intact.
    if (c == '\n' && l->last_cr) {
        l->last_cr = false;
        return TINY_LINE_NONE;
    }
    l->last_cr = (c == '\r');

    if (l->ready) {
        l->ready = false;
        l->len = 0;
        l->buf[0] = '\0';
    }

    if (c == '\r' || c == '\n') {
        l->ready = true;
        l->buf[l->len] = '\0';
        if (l->overflow) {
            l->overflow = false;
            return TINY_LINE_TOO_LONG;
        }
        return TINY_LINE_READY;
    }

    if (c == '\b' || c == 0x7f) {
        if (!l->overflow && l->len > 0) {
            l->len--;
        }
        return TINY_LINE_NONE;
    }
    if ((unsigned char)c < 0x20 && c != '\t') {
        return TINY_LINE_NONE;
    }

    if (l->len == TINY_LINE_MAX) {
        l->overflow = true;
    }
    if (!l->overflow) {
        l->buf[l->len++] = c;
    }
    return TINY_LINE_NONE;
}
