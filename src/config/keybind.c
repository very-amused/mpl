#include "keybind.h"
#include "config/functions.h"
#include "config/internal/state.h"

#include "ui/event_queue.h"
#include "util/log.h"

#include <ctype.h>

int call_keybind(wchar_t keycode) {
	// temp
	if (keycode > (unsigned char)-1) {
		return 1;
	}

	static struct seekArgs seek_args;

	switch (tolower(keycode)) {
	case 'p':
		play_toggle();
		return 0;
	case 'q':
		quit();
		return 0;
	case ',':
		seek_args.ms = -1000;
		seek_snap(&seek_args);
		return 0;
	case '.':
		seek_args.ms = 1000;
		seek_snap(&seek_args);
		return 0;
	case '<':
		seek_args.ms = -5000;
		seek_snap(&seek_args);
		return 0;
	case '>':
		seek_args.ms = 5000;
		seek_snap(&seek_args);
		return 0;
	}

	return 1;
}
