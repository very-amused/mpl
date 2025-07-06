#include "string_win32.h"

#include <stdlib.h>

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