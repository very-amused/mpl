project('MPL', 'c', meson_version : '>=1.6.0')


# Source and header files
src = []
include = [include_directories('src')]

# Dependencies
deps = []
deps += dependency('libavcodec', version : '>=60.0.0')
deps += dependency('libavformat', version : '>=60.0.0')
deps += dependency('libavutil', version : '>=58.0.0')
cmake = import('cmake')
libdill_opts = cmake.subproject_options()
libdill_opts.set_override_option('c_std', 'gnu11')
libdill_subproj = cmake.subproject('libdill', options: libdill_opts)
deps += libdill_subproj.dependency('dill')
include += libdill_subproj.include_directories('dill')

# Preprocessor defines
cflags = []
if get_option('pulseaudio').enabled()
	cflags += '-DAO_PULSEAUDIO'
endif

# Debug options
if get_option('buildtype') == 'debug'
	cflags += '-O1'
	cflags += '-fsanitize=address'
	cflags += '-fno-omit-frame-pointer'
endif

subdir('src')

executable('mpl', src,
	include_directories : include,
	dependencies : deps,
	c_args : cflags, link_args: cflags)
