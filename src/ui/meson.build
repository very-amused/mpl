src_ui = files('cli.c', 'cli_args.c', 'event_queue.c', 'timecode.c')
if build_machine.system() == 'windows'
    src_ui += files('input_thread_win32.c')
else
    src_ui += files('input_thread.c')
endif
src += src_ui
