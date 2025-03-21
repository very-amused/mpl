project('MPL', 'c', meson_version : '>=1.6.0')

# Dependencies
deps = []
deps += dependency('libavcodec', version : '>=60.0.0')
deps += dependency('libavformat', version : '>=60.0.0')
deps += dependency('libavutil', version : '>=58.0.0')

# Source and header files
src = []
include = ['src']

# Preprocessor defines
cpp_defines = []
if get_option('pulseaudio').enabled()
	cpp_defines += '-DAO_PULSEAUDIO'
endif

subdir('src')

executable('mpl', src, include_directories : include, dependencies : deps, c_args: cpp_defines)
