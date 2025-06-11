#include "strtokn.h"


int strtokn(size_t *offset, size_t *tok_len,
		const char *s, const size_t s_len, const char *delims) {
	// Move offset past previous token + delimiter
	if (*tok_len > 0) {
		*offset += *tok_len + sizeof(char);
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

int strtokn_s(strtoknState *state, const char *delims) {
	return strtokn(&state->offset, &state->tok_len, state->s, state->s_len, delims);
}
