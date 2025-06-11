#pragma once
#include <stddef.h>

/* strtokn: good string tokenization functions for parsing */

/* strtok if it was good.
*offset and *tok_len must both be 0 when first called,
and will be set to offset and length values (relative to s) of the next split token
Returns 0 on success, -1 on EOF

(c) 2024 Keith Scroggs;
Originally implemented in argon2_mariadb */
int strtokn(size_t *offset, size_t *tok_len,
		const char *s, const size_t s_len, const char *delims);

// State for the strtokn_s family of functions
typedef struct strtoknState {
	// Token offset and length
	size_t offset, tok_len;
	// String being parsed
	const char *s;
	const size_t s_len;
} strtoknState;

// Call strtokn with a state struct
int strtokn_s(strtoknState *state, const char *delims);
