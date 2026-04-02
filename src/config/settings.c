#include "settings.h"
#include "config/keybind/function.h"
#include "error.h"
#include "util/log.h"
#include "util/strtokn.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __WIN32
#include "util/compat/string_win32.h"
#endif

void Settings_init(Settings *opts) {
	memcpy(opts, &default_settings, sizeof(Settings));
}

void Settings_deinit(Settings *opts) {
	free(opts->audio_backend);
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
		static const char AUDIO_BACKEND[] = "audio_backend";
		static const char AT_BUFFER_AHEAD[] = "at_buffer_ahead";
		static const char AB_BUFFER_MS[] = "ab_buffer_ms";
		if (strcmp(optname, AUDIO_BACKEND) == 0) {
			opt_t = Settings_STR;
			opt_dst = (unsigned char *)&opts->audio_backend;
		} else if (strcmp(optname, AT_BUFFER_AHEAD) == 0) {
			opt_t = Settings_U32;
			opt_dst = (unsigned char *)&opts->at_buffer_ahead;
		} else if (strcmp(optname, AB_BUFFER_MS) == 0) {
			opt_t = Settings_U32;
			opt_dst = (unsigned char *)&opts->ab_buffer_ms;
		}
		break;
	}

	case 'u':
	{
		static const char UI_TIMECODE_MS[] = "ui_timecode_ms";
		if (strcmp(optname, UI_TIMECODE_MS) == 0) {
			opt_t = Settings_BOOL;
			opt_dst = (unsigned char *)&opts->ui_timecode_ms;
		}
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
	// discard semicolon terminator
	if (parse_state.s[parse_state.offset + (parse_state.tok_len-1)] == Keybind_DELIM) {
		parse_state.tok_len--;
	}
	if (!parse_state.tok_len) {
		return Settings_SYNTAX_ERR;
	}
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

	case Settings_BOOL:
	{
		bool *dst = (bool *)opt_dst;
		if (strcmp(val_str, "true") == 0) {
			*dst = true;
		} else if (strcmp(val_str, "false") == 0) {
			*dst = false;
		} else {
			return Settings_SYNTAX_ERR;
		}
		LOG(Verbosity_DEBUG, "Parsed setting: %s = %s (%s)\n", optname, *dst ? "true" : "false", Settings_t_name(opt_t));
		break;
	}

	case Settings_STR:
	{
		// Support both double and single quoted strings
#define ENCLOSED(s,q) (s[0] == q && val_str[parse_state.tok_len-1] == q)
		if (parse_state.tok_len < 2 ||
				!(ENCLOSED(val_str, '"') || ENCLOSED(val_str, '\''))) {
			return Settings_SYNTAX_ERR;
		}
#undef ENCLOSED
		char **dst = (char **)opt_dst;
		*dst = strndup(&val_str[1], parse_state.tok_len-2);  // Copy without enclosing quotes
		LOG(Verbosity_DEBUG, "Parsed setting: %s = '%s' (%s)\n", optname, *dst, Settings_t_name(opt_t));
		break;
	}
	}

	return Settings_OK;
}
