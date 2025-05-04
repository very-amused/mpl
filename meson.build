project('MPL', 'c', version: '0.0.1', meson_version : '>=1.6.0')

# Dependencies
deps = []
# ffmpeg
deps += dependency('libavcodec', version : '>=60.0.0')
deps += dependency('libavformat', version : '>=60.0.0')
deps += dependency('libavutil', version : '>=58.0.0')
# audio output
pulseaudio = dependency('libpulse', version : '>=16.0.0', required : get_option('pulseaudio'))
pipewire = dependency('libpipewire-0.3', version : '>=0.3.0', required : get_option('pipewire'))
deps += pulseaudio
deps += pipewire

# Source and header files
src = []
include = include_directories('src')

# Preprocessor defines
cflags = []
cflags += '-DMPL_VERSION="@0@"'.format(meson.project_version())
if pulseaudio.found()
	cflags += '-DAO_PULSEAUDIO'
endif
if pipewire.found()
	cflags += '-DAO_PIPEWIRE'
endif

# Debug options
if get_option('buildtype') == 'debug'
	cflags += '-O0'
	cflags += '-fsanitize=address'
	cflags += '-fno-omit-frame-pointer'
endif

subdir('src')

executable('mpl', src,
	include_directories : include,
	dependencies : deps,
	c_args : cflags, link_args: cflags,
	install  : true)
