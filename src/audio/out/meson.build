src_audio_out = files('backend.c')

if pulseaudio.found()
	src_audio_out += files('pulseaudio.c')
endif
if pipewire.found()
	src_audio_out += files('pipewire.c')
endif

src += src_audio_out
