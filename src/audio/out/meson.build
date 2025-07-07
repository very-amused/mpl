src_audio_out = files('backend.c')

if get_option('buildtype') == 'debug'
	src_audio_out += files('dummy.c', 'dummy/loop.c')
endif
if pulseaudio.found()
	src_audio_out += files('pulseaudio.c')
endif
if pipewire.found()
	src_audio_out += files('pipewire.c')
endif

src += src_audio_out