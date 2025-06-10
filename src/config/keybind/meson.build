src_keybind = files('keybind.c', 'keycode.c', 'function.c')
if get_option('ascii_keybinds')
	# ASCII-only keybinds done in C; more simple
	src_keybind += files('keybind_map_ascii.c')
else
	# UTF-8 keybinds done in C++; less simple (default)
	src_keybind += files('keybind_map.cpp')
endif

src += src_keybind

