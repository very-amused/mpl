src_audio = []
src_audio += files('out/backend.c')
src_audio += files('track.c', 'buffer.c', 'pcm.c')

src_audio += files('out/dummy.c')
if pulseaudio.found()
	src_audio += files('out/pulseaudio.c')
endif
if pipewire.found()
	src_audio += files('out/pipewire.c')
endif

src += src_audio

subdir('out')