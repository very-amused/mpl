#include "strtokn.h"
#include <stdbool.h>
#include <stddef.h>


int strtokn(size_t *offset, size_t *tok_len,
		const char *s, const size_t s_len, const char *delims) {
	// Move offset past previous token + delimiter
	if (*tok_len > 0) {
		*offset += *tok_len + sizeof(char);
	} else if (*offset > 0) {
		*offset += sizeof(char);
	}
	// Max token length
	const size_t max_len = s_len - *offset;
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

int strtokn_s(strtoknState *state, const char *delims) {
	return strtokn(&state->offset, &state->tok_len, state->s, state->s_len, delims);
}

int strtokn_consume_s(strtoknState *state, const char *consume) {
	return strtokn_consume(&state->offset, &state->tok_len, state->s, state->s_len, consume);
}
