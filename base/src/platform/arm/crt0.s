/**
 * \brief   Startup code for Genode applications on ARM
 * \author  Norman Feske
 * \date    2007-04-28
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
.section ".text.crt0"

	.globl _start
_start:

	ldr r4, .initial_sp
	str sp, [r4]

	ldr  sp, .stack_high
	b    _main

.initial_sp: .word __initial_sp
.stack_high: .word _stack_high

/*--------------------------------------------------*/
        .data
	.globl	__dso_handle
__dso_handle:
	.long	0

	.globl  __initial_sp
__initial_sp:
	.long   0

/*--- .bss (non-initialized data) ------------------*/
.section ".bss"

	.p2align 4
	.globl	_stack_low
_stack_low:
	.space	64*1024
	.globl	_stack_high
_stack_high:

	/*
	 * Symbol referenced by ldso's crt0.s, which is needed by base-hw only.
	 * It is defined here merely to resolve the symbol for non-base-hw
	 * platforms.
	 */
	 .globl _main_utcb
	_main_utcb: .long 0
