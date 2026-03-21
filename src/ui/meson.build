src_ui = files('cli.c', 'cli_args.c', 'input_thread.c', 'timecode.c')
if build_machine.system() == 'windows'
	src_ui += files('event_queue_win32.c')
else
	src_ui += files('event_queue.c')
endif
src += src_ui
