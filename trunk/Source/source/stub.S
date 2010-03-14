	.rodata

	.globl bgImg
	.balign 32
bgImg:
	.incbin "../data/background"

	.globl cover_top
	.balign 32
cover_top:
	.incbin "../data/cover_top"

	.globl cover_side
	.balign 32
cover_side:
	.incbin "../data/cover_side"

	.globl cover_front
	.balign 32
cover_front:
	.incbin "../data/cover_front"

	.globl coverImg
	.balign 32
coverImg:
	.incbin "../data/nocover"

	.globl coverImg_full
	.balign 32
coverImg_full:
	.incbin "../data/nocover_full"

	.globl pointer
	.balign 32
pointer:
	.incbin "../data/pointer"

	.globl hourglass
	.balign 32
hourglass:
	.incbin "../data/hourglass"

	.globl gui_font
	.balign 32
gui_font:
	.incbin "../data/gui_font"

	.globl bg_gui
	.balign 32
bg_gui:
	.incbin "../data/bg_gui"

	.globl star_icon
	.balign 32
star_icon:
	.incbin "../data/star"

#;	.globl ehcmodule2
#;	.balign 32
#;ehcmodule2:
#;	.incbin "../data/ehcmodule2_elf"

	.globl dip_plugin2
	.balign 32
dip_plugin2:
	.incbin "../data/dip_plugin2_bin"

#;	.globl ehcmodule3
#;	.balign 32
#;ehcmodule3:
#;	.incbin "../data/ehcmodule3_elf"

	.globl dip_plugin3
	.balign 32
dip_plugin3:
	.incbin "../data/dip_plugin3_bin"

#;	.globl ehcmodule4
#;	.balign 32
#;ehcmodule4:
#;	.incbin "../data/ehcmodule4_elf"

	.globl dip_plugin4
	.balign 32
dip_plugin4:
	.incbin "../data/dip_plugin4_bin"

	.globl ehcmodule_frag
	.balign 32
ehcmodule_frag:
	.incbin "../data/ehcmodule_frag"

	.globl introImg
	.balign 32
introImg:
	.incbin "../data/intro2"

##; commented out
#;	.globl opening_bnr
#;	.balign 32
#;opening_bnr:
#;	.incbin "../data/opening_bnr"

	.globl dip_plugin_249
	.balign 32
dip_plugin_249:
	.incbin "../data/dip_plugin_249"


