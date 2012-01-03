/**
 * Assembler macros for machine status register access
 *
 * \author  Martin Stein
 * \date  2010-10-05
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.macro _SYNCHRONIZING_OP
	bri 4
.endm


.macro _AWAIT_DELAY_OP
	or r0, r0, r0
.endm


.macro _ENABLE_EXCEPTIONS
	msrset r0, 0x100
	_SYNCHRONIZING_OP
.endm


.macro _DISABLE_EXCEPTIONS
	msrclr r0, 0x100
	_SYNCHRONIZING_OP
.endm


.macro _ENABLE_INTERRUPTS
	msrset r0, 0x002
	_SYNCHRONIZING_OP
.endm


.macro _DISABLE_INTERRUPTS
	msrclr r0, 0x002
	_SYNCHRONIZING_OP
.endm


.macro _RELEASE_EXCEPTION
	msrclr r0, 0x200
	_SYNCHRONIZING_OP
.endm


.macro _RELEASE_BREAK
	msrclr r0, 0x008
	_SYNCHRONIZING_OP
.endm

