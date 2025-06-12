#include "strtokn.h"
#include <stdbool.h>
#include <stddef.h>

void strtokn_init(StrtoknState *state, const char *s, size_t s_len) {
	state->s = s;
	state->s_len = s_len;

	state->offset = state->tok_len = 0;
	state->delim_read = false;
}

int strtokn(StrtoknState *state, const char *delims) {
	// Move offset past previous token + delim
	state->offset += state->tok_len;
	if (state->delim_read) {
		state->offset += sizeof(char);
	}
	// Reset token length
	state->tok_len = 0;
	// Compute max token length we can parse and token start
	const size_t max_len = state->s_len - state->offset;
	const char *start = state->s + state->offset;

	// Search until the next token is found or the string ends
	while (state->tok_len < max_len) {
		const char c = start[state->tok_len];
		for (const char *delim = &delims[0]; *delim != '\0'; delim++) {
			if (c == *delim) {
				state->delim_read = true;
				return 0;
			}
		}
		state->tok_len++;
	}

	return -1;
}

int strtokn_consume(StrtoknState *state, const char *consume) {
	// Advance past previous token + delim
	state->offset += state->tok_len;
	if (state->delim_read) {
		state->offset += sizeof(char);
	}
	// Reset token and delim state (since we aren't reading up to a delim)
	state->tok_len = 0;
	state->delim_read = false;
	// Compute max token length we can parse and token start
	const size_t max_len = state->s_len - state->offset;
	const char *start = state->s + state->offset;

	// Consume until we find a char not in *consume,
	// then advance past the token we consumed
	while (state->tok_len < max_len) {
		const char c = start[state->tok_len];
		bool consume_char = false;
		for (const char *delim = &consume[0]; *delim != '\0'; delim++) {
			if (c == *delim) {
				consume_char = true;
				break;
			}
		}

		if (!consume_char) {
			state->offset += state->tok_len;
			state->tok_len = 1;
			return 0;
		}
		state->tok_len++;
	}

	return -1;
}
