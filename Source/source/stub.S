	.rodata

	.globl bgImg
	.balign 32
bgImg:
	.incbin "../data/background"

	.globl bgImg_wide
	.balign 32
bgImg_wide:
	.incbin "../data/background_wide"

	.globl coverImg
	.balign 32
coverImg:
	.incbin "../data/nocover"

	.globl coverImg_wide
	.balign 32
coverImg_wide:
	.incbin "../data/nocover_wide"

	.globl pointer
	.balign 32
pointer:
	.incbin "../data/pointer"

	.globl hourglass
	.balign 32
hourglass:
	.incbin "../data/hourglass"

	.globl console_font
	.balign 32
console_font:
	.incbin "../data/console_font"

	.globl bg_gui
	.balign 32
bg_gui:
	.incbin "../data/bg_gui"

	.globl star_icon
	.balign 32
star_icon:
	.incbin "../data/star"

	.globl ehcmodule
	.balign 32
ehcmodule:
	.incbin "../data/ehcmodule_elf"

	.globl dip_plugin
	.balign 32
dip_plugin:
	.incbin "../data/dip_plugin_bin"


