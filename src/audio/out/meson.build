src_audio_out = files('backend.c', 'auto.c')

if pulseaudio.found()
	src_audio_out += files('pulseaudio.c')
endif
if pipewire.found()
	src_audio_out += files('pipewire.c')
endif
if enable_wasapi
  src_audio_out += files('wasapi.c', 'wasapi_fb_thread.c')
endif
if get_option('ao_fast')
	src_audio_out += files('debug_fast.c')
endif

src += src_audio_out
