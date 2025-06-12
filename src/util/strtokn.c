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

int strtokn_consume_s(StrtoknState *state, const char *consume) {
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

int strtokn_consume(size_t *offset, size_t *tok_len,
		const char *s, const size_t s_len, const char *consume) {
	// Move offset past previous token + delimiter
	if (*tok_len > 0) {
		*offset += *tok_len + sizeof(char);
	} else if (*offset > 0) {
		*offset += sizeof(char);
	}
	// Max number of chars we can consume before EOF
	const size_t max_len = s_len - *offset;
	// Token start
	const char *start = s + *offset;
	// Reset current token length
	*tok_len = 0;

	// Consume chars until a char NOT marked for consumption is found
	size_t n_consumed = 0;
	while (n_consumed < max_len) {
		const char c = start[*offset];
		bool is_consumable = false;
		for (const char *consumable = &consume[0]; *consumable != '\0'; consumable++) {
			if (c == *consumable) {
				is_consumable = true;
				break;
			}
		}
		if (!is_consumable) {
			(*tok_len) = 1;
			return 0;
		}
		(*offset)++;
		n_consumed++;
	}


	return -1;
}
