src_config = files('config.c', 'settings.c')

if build_machine.system() == 'linux'
    # WARNING: due to meson not understanding .incbin and
    # **not providing an escape hatch to directly declare incremental build depfiles**,
    # changes to the default mpl.conf do NOT trigger a rebuild.
    # Thus, we must clean and recompile to apply changes to the default mpl.conf (for now)
    # See https://github.com/mesonbuild/meson/issues/9849
    src_config += files('default_config.s')
endif


subdir('internal')
subdir('keybind')
if get_option('config_functions_next')
	subdir('function')
else
	src_config += files('function.sc')
endif

src += src_config
