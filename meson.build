project('MPL', 'c', 'cpp',
	version: '0.0.4', meson_version : '>=1.6.0',
	license : 'GPL-3.0-only',
	license_files: ['LICENSE'])

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
# POSIX threading API
pthread = dependency('threads')
deps += pthread

# Source and header files
src = []
include = include_directories('src')

# Preprocessor defines and compiler overrides
cflags = []
cflags += '-DMPL_VERSION="@0@"'.format(meson.project_version())
override_options = []
# if we don't have .incbin, we need #embed
if build_machine.system() != 'linux'
	cflags += '--embed-dir=@0@'.format(meson.project_source_root())
	override_options += 'c_std=c23'
endif
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
	cflags += '-DMPL_DEBUG'
endif
if get_option('test_config')
	cflags += '-DMPL_TEST_CONFIG'
endif

subdir('src')

executable('mpl', src,
	include_directories : include,
	dependencies : deps,
	c_args : cflags, link_args : cflags, override_options : override_options,
	install  : true)
