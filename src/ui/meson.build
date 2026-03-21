src_ui = files('cli.c', 'cli_args.c', 'timecode.c')
if build_machine.system() == 'windows'
	src_ui += files('event_queue_win32.c')
  src_ui += files('input_thread_win32.c')
else
	src_ui += files('event_queue.c')
  src_ui += files('input_thread.c')
src += src_ui
