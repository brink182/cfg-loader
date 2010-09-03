/*
 * DIP plugin for Custom IOS.
 *
 * Copyright (C) 2008-2010 Waninkoko, WiiGator.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

	.text

	.align 4
	.code 32

/* Syscalls */
        .global Direct_Syscall
Direct_Syscall:
	ldr	r12, =ios
	ldr	r12, [r12]
	nop
	ldr	r12, [r12, r11, lsl#2]
	nop
	bx	r12


/* Invalidate/Flush cache */
	.global DCInvalidateRange
DCInvalidateRange:
	mov     r11, #0x3f
	b	Direct_Syscall

	.global DCFlushRange
DCFlushRange:
	mov     r11, #0x40
	b	Direct_Syscall

	.global ICInvalidate
ICInvalidate:
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0
	bx	lr


/* Access permissions */
	.global Perms_Read
Perms_Read:
	mrc	p15, 0, r0, c3, c0
	bx	lr

	.global Perms_Write
Perms_Write:
	mcr	p15, 0, r0, c3, c0
	bx	lr


/* SWI mload */
	.global Swi_MLoad
Swi_MLoad:
	svc	0xcc
	bx	lr


/* Address conversion */
	.global VirtToPhys
VirtToPhys:
	and	r0, #0x7fffffff
	bx	lr

	.global PhysToVirt
PhysToVirt:
	orr	r0, #0x80000000
	bx	lr
