src_keybind = files('keycode.c')

if get_option('config_functions_next')
	if get_option('ascii_keybinds')
		# ASCII-only keybinds done in C; more simple
		src_keybind += files('keybind_map_ascii_new.c')
	else
		# UTF-8 keybinds done in C++; less simple (default)
		src_keybind += files('keybind_map_new.cpp')
	endif
else
	src_keybind += files('function.c')
	if get_option('ascii_keybinds')
		# ASCII-only keybinds done in C; more simple
		src_keybind += files('keybind_map_ascii.c')
	else
		# UTF-8 keybinds done in C++; less simple (default)
		src_keybind += files('keybind_map.cpp')
	endif
endif

src += src_keybind

