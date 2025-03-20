project('MPL', 'c', meson_version : '>=1.6.0')

# Dependencies
deps = []
deps += dependency('libavcodec', version : '>=60.0.0')
deps += dependency('libavformat', version : '>=60.0.0')
deps += dependency('libavutil', version : '>=58.0.0')

# Source and header files
src = []
include = ['src']

# Environment
env = environment()
env.set('DEBUG', 1)

subdir('src')

executable('mpl', src, include_directories : include, dependencies : deps)
