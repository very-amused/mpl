#pragma once
#include <stdbool.h>
#include <stddef.h>

/* strtokn: good string tokenization functions for parsing */


// State for the strtokn family of functions
// Initialize with [strtokn_init]
typedef struct StrtoknState {
	// Token offset and length
	size_t offset, tok_len;
	// String being parsed
	const char *s;
	size_t s_len;
	// Whether a delimiter has been read, implying we should advance parsing past it
	bool delim_read;
} StrtoknState;

// Initialize StrtoknState. Must be called before any other strtokn family functions.
void strtokn_init(StrtoknState *state, const char *s, const size_t s_len);


// Consume (ignore) every character in [consume] (starting at *offset+*tok_len) until a a char NOT in [consume] is reached, which will be the token returned.
//
// Returns 0 on success, -1 on EOF
int strtokn_consume(size_t *offset, size_t *tok_len,
		const char *s, const size_t s_len, const char *consume);

/*
strtok if it was good. initialize state with [strtokn_init] before first call.
each call will result in state->offset and state->tok_len being set to offset and length values
(relative to state->s) of the next split token.
Returns 0 on success, -1 on EOF

(c) 2024 Keith Scroggs;
Originally implemented in argon2_mariadb
*/
int strtokn(StrtoknState *state, const char *delims);

// Call strtokn_consume with a state struct
int strtokn_consume_s(StrtoknState *state, const char *consume);
