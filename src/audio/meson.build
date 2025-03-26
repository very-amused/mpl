src_audio = []
src_audio += files('out/backend.c')
src_audio += files('track.c', 'buffer.c', 'pcm.c')

if get_option('pulseaudio').enabled()
	deps += dependency('libpulse', version : '>=16.0.0')
	src_audio += files('out/pulseaudio.c')
endif

slib_audio = static_library('audio.a', src_audio, include_directories : include, c_args : cflags, link_args : cflags)
deps += declare_dependency(include_directories : include, link_with : slib_audio, sources : src_audio)
