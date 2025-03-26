src_queue = files('queue.c', 'lock.c')

slib_queue = static_library('queue.a', src_queue, include_directories : include, c_args : cflags, link_args : cflags)
deps += declare_dependency(include_directories : include, link_with : slib_queue, sources : src_queue)
