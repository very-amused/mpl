src_audio_out = files('backend.c', 'default.c')

if pulseaudio.found()
	src_audio_out += files('pulseaudio.c')
endif
if pipewire.found()
	src_audio_out += files('pipewire.c')
endif
if get_option('ao_fast')
	src_audio_out += files('debug_fast.c')
endif

src += src_audio_out
