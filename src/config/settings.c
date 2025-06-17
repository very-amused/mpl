#include "settings.h"
#include "error.h"
#include "util/log.h"
#include "util/strtokn.h"

#include <stdio.h>
#include <string.h>


void Settings_init(Settings *opts) {
	memcpy(opts, &default_settings, sizeof(Settings));
}

void Settings_deinit(Settings *opts) {
	(void)0;
}

enum Settings_ERR Settings_parse_setting(Settings *opts, const char *line) {
	StrtoknState parse_state;
	strtokn_init(&parse_state, line, strlen(line));

	// {optname}
	if (strtokn(&parse_state, WHITESPACE) != 0) {
		return Settings_SYNTAX_ERR;
	}
	char optname[parse_state.tok_len + 1];
	strncpy(optname, &parse_state.s[parse_state.offset], parse_state.tok_len);
	optname[parse_state.tok_len] = '\0';


	// Type of the setting value being parsed
	unsigned char *opt_dst = NULL;
	enum Settings_t opt_t = -1;

	switch (optname[0]) {
	case 'a':
	{
		static const char AT_BUFFER_AHEAD[] = "at_buffer_ahead";
		static const char AB_BUFFER_MS[] = "ab_buffer_ms";
		if ((strcmp(optname, AT_BUFFER_AHEAD)) == 0) {
			opt_t = Settings_U32;
			opt_dst = (unsigned char *)&opts->at_buffer_ahead;
		} else if (strcmp(optname, AB_BUFFER_MS) == 0) {
			opt_t = Settings_U32;
			opt_dst = (unsigned char *)&opts->ab_buffer_ms;
		}
		break;
	}
	}


	// =
	if (strtokn(&parse_state, WHITESPACE) != 0) {
		return Settings_SYNTAX_ERR;
	}

	if (opt_t == -1) {
		return Settings_UNKNOWN;
	}

	// {val}
	strtokn(&parse_state, WHITESPACE);
	char val_str[parse_state.tok_len + 1];
	strncpy(val_str, &parse_state.s[parse_state.offset], parse_state.tok_len);
	val_str[parse_state.tok_len] = '\0';


	switch (opt_t) {
	case Settings_U32:
	{
		uint32_t *dst = (uint32_t *)opt_dst;
		if (sscanf(val_str, "%u", dst) != 1) {
			return Settings_SYNTAX_ERR;
		}
		LOG(Verbosity_DEBUG, "Parsed setting: %s = %u (%s)\n", optname, *dst, Settings_t_name(opt_t));
		break;
	}
	}

	return Settings_OK;
}
