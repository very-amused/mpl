src_ui_cli = []

if build_machine.system() == 'windows'
	src_ui_cli += files('input_thread_win32.c')
else
	src_ui_cli += files('input_thread.c')
endif

src += src_ui_cli
