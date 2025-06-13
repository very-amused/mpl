src_config = files('functions.c', 'config.c')
# WARNING: due to meson not understanding .incbin and
# **not providing an escape hatch to directly declare incremental build depfiles**,
# changes to the default mpl.conf do NOT trigger a rebuild.
# Thus, we must clean and recompile to apply changes to the default mpl.conf (for now)
# See https://github.com/mesonbuild/meson/issues/9849
src_config += files('default_config.s')
src += src_config

subdir('keybind')
