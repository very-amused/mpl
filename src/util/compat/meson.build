src_util_compat = []
if build_machine.system() == 'windows'
    # Implementations of POSIX string functions (e.g strndup, getline)
    src_util_compat += files('string_win32.c')
endif
src += src_util_compat