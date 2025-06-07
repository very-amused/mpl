#include "keybinds.h"
#include "config/functions.h"
#include "util/log.h"

#include <ctype.h>

int call_keybind(wchar_t keycode) {
	// temp
	if (keycode > (unsigned char)-1) {
		return 1;
	}

	switch (tolower(keycode)) {
	case 'p':
		play_toggle();
		return 0;
	case 'q':
		break;
		//goto quit;
	}

	return 1;
}
