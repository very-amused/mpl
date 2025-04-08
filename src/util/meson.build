src_util = files('rational.c')

slib_util = static_library('util.a', src_util, include_directories: include, c_args: cflags, link_args: cflags)
deps += declare_dependency(include_directories: include, link_with: slib_util, sources: src_util)
