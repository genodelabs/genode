/*
 * \brief  Linker script for Genode programs
 * \author Christian Helmuth
 * \date   2006-04-12
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

ENTRY(_start)

PHDRS
{
  ro PT_LOAD FLAGS(5);
  rw PT_LOAD FLAGS(6);
}

SECTIONS
{
	. = 0x90000000; 
	_program_image_begin = .;
	_executable_readable_begin = .;

	.text : {
		*(.Atomic_ops)

		. = ALIGN(1<<12);
		*(.Elf_entry)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.fini)
		*(.rodata .rodata.* .gnu.linkonce.r.*)

		. = ALIGN(1<<3);
		_ctors_start = .;
		KEEP (*(.ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (*(.init_array)) /* list of constructors specific for ARM eabi */
		_ctors_end = .;
		_dtors_start = .;
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
		_dtors_end = .;

	} : ro = 0x90909090

	/* Linux: exception section for uaccess mechanism */
	__ex_table : { *(__ex_table) }

	.eh_frame_hdr : { *(.eh_frame_hdr) }
	_executable_readable_end = .;

	. = ALIGN(1<<12);
	_writable_readable_begin = .;

	.data : SUBALIGN(1<<12) {
		/*
		 * Leave space for parent capability parameters at start of data
		 * section. The protection domain creator is reponsible for storing
		 * sane values here.
		 */
		_parent_cap = .;
		_parent_cap_thread_id = .;
		LONG(0xffffffff);
		_parent_cap_local_name = .;
		LONG(0xffffffff);

		*(.data .data.* .gnu.linkonce.d.*)
	} : rw

	/* exception frames for C++ */
	.eh_frame : {
		__eh_frame_start__ = .;
		KEEP (*(.eh_frame))
		LONG(0)
	} : rw

	.init_array : {
		__init_array_start = .;
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array))
		__init_array_end = .;
	}

	.gcc_except_table : { KEEP(*(.gcc_except_table)) }
	.dynamic :  { *(.dynamic) }

	/* .ARM.exidx is sorted, so has to go in its own output section */
	__exidx_start = .;
	.ARM.exidx : {
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	}
	__exidx_end = .;

	.ARM.extab : {
		*(.ARM.extab*)
	} : rw

	.bss : {
		*(.bss .bss.* .gnu.linkonce.b.* COMMON)
	}

	/* end of program image -- must be after last section */
	_writeable_readable_end = .;
	_program_image_end = .;

	/DISCARD/ : {
		*(.note)
		*(.note.ABI-tag)
		*(.comment)
	}
}
