src += files('out/backend.c')
src += files('track.c', 'buffer.c', 'pcm.c')

if get_option('pulseaudio').enabled()
	deps += dependency('libpulse', version : '>=16.0.0')
	src += files('out/pulseaudio.c')
endif
