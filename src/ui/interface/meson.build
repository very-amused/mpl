src_ui_interface = files('interface.c', 'auto.c')

if get_option('cli').allowed()
	src_ui_interface += files('cli.c')
endif

src += src_ui_interface
