project('MPL', 'c', 'cpp',
	version: '0.4.4', meson_version : '>=1.6.0',
	license : 'GPL-3.0-only',
	license_files: ['LICENSE'])

is_win32 = build_machine.system() == 'windows'
enable_wasapi = is_win32 and get_option('wasapi').allowed()
# Only enable resampling when supporting AudioBackend's that require it.
# We leave resampling up to the audio server whenever possible.
enable_resampling = enable_wasapi or get_option('test_resampling')

# Dependencies
deps = []
# ffmpeg
deps += dependency('libavcodec', version : '>=60.0.0')
deps += dependency('libavformat', version : '>=60.0.0')
deps += dependency('libavutil', version : '>=58.0.0')
if enable_resampling
	deps += dependency('libswresample', version : '>=6.0.0')
endif
# audio output
pulseaudio = dependency('libpulse', version : '>=16.0.0', required : get_option('pulseaudio'))
pipewire = dependency('libpipewire-0.3', version : '>=0.3.0', required : get_option('pipewire'))
deps += pulseaudio
deps += pipewire
# POSIX threading API
pthread = dependency('threads')
deps += pthread
# Fake Audio Server for Testing (FAST)
if get_option('ao_fast')
	libfast_proj = subproject('libfast')
	fast = libfast_proj.get_variable('libfast_dep')
	deps += fast
endif

# Source and header files
src = []
include = []
include += include_directories('src')
if is_win32
	include += include_directories('src/util/compat')
endif

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
if enable_resampling
	cflags += '-DMPL_RESAMPLE'
	if get_option('test_resampling')
		cflags += '-DMPL_RESAMPLE_PHONY'
	endif
endif
cc = meson.get_compiler('c')
if enable_wasapi
  cflags += '-DAO_WASAPI'
  # Provides KSDATAFORMAT_SUBTYPE idents
  deps += cc.find_library('ksuser', required : true)
endif
if get_option('ao_fast')
	cflags += '-DAO_FAST'
endif

# Debug options
if get_option('buildtype') == 'debug'
	cflags += '-O0'
	if build_machine.system() == 'linux'
		cflags += '-fsanitize=address'
		cflags += '-fno-omit-frame-pointer'
	endif
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
