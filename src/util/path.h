#pragma once
#include <stdarg.h>
#include <stddef.h>

// Join paths and place the result in *dst.
// *dst is allocated using [malloc], and *dst_len is set.
// Returns 0 on success, 1 if no paths were provided.
int path_join(char **dst, size_t *dst_len,
		const char **subpaths, const size_t n_subpaths);
