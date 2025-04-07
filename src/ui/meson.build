src_ui = files('cli.c', 'ncurses.c')

slib_ui = static_library('ui.a', src_ui, include_directories: include, c_args: cflags, link_args: cflags)
deps += declare_dependency(include_directories: include, link_with: slib_ui, sources: src_ui)
