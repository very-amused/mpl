project('MPL', 'c', 'cpp',
	version: '0.5.0', meson_version : '>=1.6.0',
	license : 'GPL-3.0-only',
	license_files: ['LICENSE'])

cc = meson.get_compiler('c')


# Auto flags
is_win32 = build_machine.system() == 'windows'
enable_wasapi = is_win32 and get_option('wasapi').allowed()
# Only enable resampling when supporting AudioBackend's that require it.
# We leave resampling up to the audio server whenever possible.
enable_resampling = enable_wasapi or get_option('test_resampling')
# Rely on sysv struct padding convention when the compiler implements it
struct_padding_testresult = cc.run(files('feature-tests/struct_padding.c')[0])
enable_known_struct_padding = struct_padding_testresult.compiled() and struct_padding_testresult.returncode() == 0

# Preprocessor defines and compiler overrides
cflags = []
cppflags = []
cflags += '-DMPL_VERSION="@0@"'.format(meson.project_version())
override_options = []
# if we don't have .incbin, we need #embed
if build_machine.system() != 'linux'
	cflags += '--embed-dir=@0@'.format(meson.project_source_root())
	override_options += 'c_std=c23'
endif
if enable_known_struct_padding
	cflags += '-DKNOWN_STRUCT_PADDING'
endif
# user interface
if get_option('cli').allowed()
	cflags += '-DUI_CLI'
endif
# Nerdfont icon support
if get_option('nerdfont_icons')
	cflags += '-DNERDFONT'
endif

# Dependencies
deps = []
# ffmpeg
deps += dependency('libavcodec', version : '>=60.0.0')
deps += dependency('libavformat', version : '>=60.0.0')
deps += dependency('libavutil', version : '>=58.0.0')
if enable_resampling
	deps += dependency('libswresample', version : '>=6.0.0')
	cflags += '-DMPL_RESAMPLE'
	if get_option('test_resampling')
		cflags += '-DMPL_RESAMPLE_PHONY'
	endif
endif
# readline (used for shell)
deps += dependency('readline', version : '>=8.3')
# audio output
pulseaudio = dependency('libpulse', version : '>=16.0.0', required : get_option('pulseaudio'))
deps += pulseaudio
if pulseaudio.found()
	cflags += '-DAO_PULSEAUDIO'
endif
pipewire = dependency('libpipewire-0.3', version : '>=0.3.0', required : get_option('pipewire'))
deps += pipewire
if pipewire.found()
	cflags += '-DAO_PIPEWIRE'
endif
if enable_wasapi
  cflags += '-DAO_WASAPI'
  # Provides KSDATAFORMAT_SUBTYPE idents
  deps += cc.find_library('ksuser', required : true)
endif
# POSIX threading API
pthread = dependency('threads')
deps += pthread
# Fake Audio Server for Testing (FAST)
if get_option('ao_fast')
	libfast_proj = subproject('libfast')
	fast = libfast_proj.get_variable('libfast_dep')
	deps += fast
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
if get_option('parsing_debug')
	cflags += '-DMPL_PARSING_DEBUG'
endif

# Source and header files
src = []
include = []
include += include_directories('src')
if is_win32
	include += include_directories('src/util/compat')
endif

subdir('src')

executable('mpl', src,
	include_directories : include,
	dependencies : deps,
	c_args : cflags, cpp_args : cppflags, link_args : cflags,
	override_options : override_options,
	install  : true)
