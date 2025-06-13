.section .rodata

.global mpl_default_config
.type mpl_default_config, @object
mpl_default_config:
	.incbin "mpl.conf"
mpl_default_config_end:
	.byte 0 # Null terminate

.global mpl_default_config_len
.type mpl_default_config_len, @object
.balign 8
mpl_default_config_len:
	.quad mpl_default_config_end - mpl_default_config
