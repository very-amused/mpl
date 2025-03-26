project('MPL', 'c', meson_version : '>=1.6.0')

# Dependencies
deps = []
deps += dependency('libavcodec', version : '>=60.0.0')
deps += dependency('libavformat', version : '>=60.0.0')
deps += dependency('libavutil', version : '>=58.0.0')

# Source and header files
src = []
include = include_directories('src')

# Preprocessor defines
cflags = []
if get_option('pulseaudio').enabled()
	cflags += '-DAO_PULSEAUDIO'
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
	c_args : cflags, link_args: cflags)
