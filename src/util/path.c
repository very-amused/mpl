#include "path.h"

#include <string.h>
#include <stdlib.h>


int path_join(char **dst, size_t *dst_len,
		const char **subpaths, const size_t n_subpaths) {
	if (n_subpaths == 0) {
		return 1;
	}

	static const char PATH_SEP = '/';
	*dst_len = n_subpaths - 1; // use path sep to join
	
	size_t subpath_lens[n_subpaths];

	// Pass 1: compute the length of our joined path
	for (size_t i = 0; i < n_subpaths; i++) {
		subpath_lens[i] = strlen(subpaths[i]);
		*dst_len += subpath_lens[i];
	}

	// Pass 2: write the joined path itself
	*dst = malloc((*dst_len + 1) * sizeof(char));
	size_t n = 0;
	for (size_t i = 0; i < n_subpaths; i++) {
		strncpy(&(*dst)[n], subpaths[i], subpath_lens[i]);
		n += subpath_lens[i];
		if (i < n_subpaths-1) {
			(*dst)[n] = PATH_SEP;
			n++;
		}
	}
	(*dst)[*dst_len] = '\0';

	return 0;
}
