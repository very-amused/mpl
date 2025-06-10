#pragma once
#include <stddef.h>

// strtok if it was good.
// *offset and *tok_len must both be 0 when first called,
// and will be set to offset and length values (relative to s) of the next split token
// Returns 0 on success, -1 on EOF
//
// (c) 2024 Keith Scroggs;
// Originally implemented in argon2_mariadb
static inline int strtokn(size_t *offset, size_t *tok_len,
		const char *s, const size_t s_len, const char *delims) {
	// Move offset past previous token + delimiter
	if (*tok_len > 0) {
		*offset += *tok_len + sizeof(char);
	}
	// Max token length
	const char max_len = s_len - *offset;
	// Token start
	const char *start = s + *offset;
	// Reset current token length
	*tok_len = 0;

	// Search until the next token is found or the string ends
	while (*tok_len < max_len) {
		const char c = start[*tok_len];
		for (const char *delim = &delims[0]; *delim != '\0'; delim++) {
			if (c == *delim) {
				return 0;
			}
		}
		(*tok_len)++;
	}

	return -1;
}
