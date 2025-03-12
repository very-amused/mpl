project('MPL', 'c', meson_version : '>=1.6.0')

# Dependencies
deps = []
deps += dependency('libavcodec', version : '>=60.0.0')

# Source and header files
src = []
include = ['src']

subdir('src')

executable('mpl', src, include_directories : include, dependencies : deps)
