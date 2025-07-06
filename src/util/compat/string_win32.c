#include "string_win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, const size_t n) {
    char *copy = malloc((n + 1) + sizeof(char));

    for (size_t i = 0; i < n; i++) {
        copy[i] = s[i];
        if (s[i] == '\0') {
            break;
        }
    }
    copy[n] = '\0';

    return copy;
}

ssize_t getline(char **line, size_t *line_len, FILE *stream) {
    static const char LF = '\n';
    // Line buffer used with fgets
    static char buf[BUFSIZ];

    // Ensure our destination can hold the line contents
    size_t line_cap = *line_len;
    if (*line == NULL || line_cap < sizeof(buf)) {
        line_cap = sizeof(buf);
        *line = realloc(*line, line_cap);
    }

    // Use our buffer to read a LF char
    *line_len = 0;
    bool has_content = false;
    while (fgets(buf, sizeof(buf), stream)) {
        has_content = true;
        size_t i = 0;
        for (; buf[i] != '\0'; i++) {
            (*line)[*line_len] = buf[i];
            (*line_len)++;
            if (buf[i] == LF) {
                return *line_len;
            }
        }
        // We didn't read a newline, so we need to grow *line, refill the buffer, and try again
        if (line_cap < (*line_len) + sizeof(buf)) {
            line_cap *= 2;
            *line = realloc(*line, line_cap);
        }
    }

    return has_content ? *line_len : EOF;
}
