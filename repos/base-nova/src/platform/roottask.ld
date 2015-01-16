/*
 * \brief  Linker script for Genode programs
 * \author Christian Helmuth
 * \date   2006-04-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * NOVA-specific change: NOVA does not support the BSS segment for
 * roottask. Therefore, we have to place all BSS content into the
 * data section.
 */

ENTRY(_start)

PHDRS
{
	ro PT_LOAD;
	rw PT_LOAD;
}

SECTIONS
{
	.text : {
		/* begin of program image (link address) */
		_prog_img_beg = .;

		/* put entry code at the start of the text segment / raw binary */
		*(.text.crt0)

		*(.init)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.fini)
		*(.rodata .rodata.* .gnu.linkonce.r.*)

		. = ALIGN(0x08);

		_ctors_start = .;
		KEEP (*(.ctors))
		KEEP (*(SORT(.ctors.*)))
		_ctors_end = .;
		_dtors_start = .;
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
		_dtors_end = .;
	} : ro = 0x0

	/* Linux: exception section for uaccess mechanism */
	__ex_table : { *(__ex_table) }

	.eh_frame_hdr : { *(.eh_frame_hdr) }

	. = ALIGN(0x1000);

	_prog_img_data = .;

	.data : {
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
		*(.bss .bss.* .gnu.linkonce.b.* COMMON)
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

	.gcc_except_table : {
		KEEP(*(.gcc_except_table))
		KEEP(*(.gcc_except_table.*))
	}
	.dynamic :  { *(.dynamic) }

	/* end of program image -- must be after last section */
	_prog_img_end = .;

	/DISCARD/ : {
		*(.note)
		*(.note.ABI-tag)
		*(.comment)
	}
}
