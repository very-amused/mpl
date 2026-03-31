src_ui = files('cli.c', 'cli_args.c', 'timecode.c')

if build_machine.system() == 'windows'
	src_ui += files('input_thread_win32.c')
else
	src_ui += files('input_thread.c')
endif

if get_option('new_event_queue')
	src_ui += files('event_queue_new.c')
else
	if build_machine.system() == 'windows'
		src_ui += files('event_queue_win32.c')
	else
		src_ui += files('event_queue_mqueue.c')
	endif
endif

src += src_ui
