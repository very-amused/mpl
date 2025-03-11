project('MPL', 'c')

# Dependencies
deps = []
deps += dependency('libpulse', version: '>=16.0.0')

# Source and header files
src = []
include = ['src']

subdir('src')

executable('mpl', src, include_directories : include)
