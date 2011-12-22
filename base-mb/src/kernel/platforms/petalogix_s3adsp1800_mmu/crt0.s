/*
 * \brief  Startup code for programs on microblaze
 * \author Martin Stein
 * \date   21.06.2010
 */

.include "linker_commands.s"
.include "errors.s"

.extern _main

.global _main_utcb_addr


.macro _INIT_MAIN_UTCB
	swi r31, r0, _main_utcb_addr
.endm


.macro _INIT_MAIN_STACK
	la r1, r0, _main_stack_base
.endm


_BEGIN_ELF_ENTRY_CODE

	_INIT_MAIN_UTCB
	_INIT_MAIN_STACK

	bralid r15, _main
	or r0, r0, r0

	_ERROR_NOTHING_LEFT_TO_CALL_BY_CRT0


_BEGIN_READABLE_WRITEABLE

	.align 4
	_main_utcb_addr: .space 1*4
	.align 4
	_main_stack_top: .space 1024*1024
	_main_stack_base:



