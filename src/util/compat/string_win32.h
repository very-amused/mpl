#ifdef __WIN32
#pragma once

// What is this header and why is it needed for ssize_t ??
#include <corecrt.h>
#include <stddef.h>
#include <stdio.h>

// Duplicate up to `n` characters (not including the null terminator) of string s,
// returning the copied char * pointer (allocated via malloc)
char *strndup(const char *s, const size_t n);

/* Read a LF-terminated line of input from `f`,
storing the line's content and length in *line and *line_len respectively.
*line is allocated and reallocated using malloc as needed.
Calling with *line == NULL is valid and expected behavior.

Returns the number of characters read, including the delimiter (LF), but not including the terminating byte.
Returns EOF when there are no more lines to be read from `f`.
*/
ssize_t getline(char **line, size_t *line_len, FILE *stream);
#endif