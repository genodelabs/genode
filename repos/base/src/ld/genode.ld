/*
 * \brief  Linker script for Genode programs
 * \author Christian Helmuth
 * \date   2006-04-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

ENTRY(_start)

PHDRS
{
	ro PT_LOAD FLAGS(5);
	rw PT_LOAD FLAGS(6);
	boot PT_LOAD FLAGS(4);
}

SECTIONS
{
	.text : {
		/* begin of program image (link address) */
		_prog_img_beg = .;

		/* put entry code at the start of the text segment / raw binary */
		KEEP (*(.text.crt0))

		*(.init)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.fini)
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	} : ro = 0x0

	. = ALIGN(0x08);

	.ctors : {
		_ctors_start = .;
		KEEP (*(.ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (*(.init_array)) /* list of constructors specific for ARM eabi */
		_ctors_end = .;
	} : ro = 0x0
	.dtors : {
		_dtors_start = .;
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
		_dtors_end = .;
	} : ro = 0x0

	/* Linux: exception section for uaccess mechanism */
	__ex_table : { *(__ex_table) }

	.eh_frame_hdr : { *(.eh_frame_hdr) }

	. = ALIGN(0x1000);

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
		LONG(0xffffffff);
		LONG(0xffffffff);
		LONG(0xffffffff);
		LONG(0xffffffff);
		LONG(0xffffffff);
		LONG(0xffffffff);

		/*
		 * Platform-specific entry for Fiasco.OC.
		 *
		 * PIC-code compiled for Fiasco.OC, needs some PIC-compatible
		 * way to enter the kernel, the fixed address of the kernel
		 * entry code address needs to be found here.
		 */
		__l4sys_invoke_indirect = .;
		LONG(0xeacff000);

		*(.data .gnu.linkonce.d.*)

		__dso_handle = .;
		LONG(0x0);

		/* include all data subsections except those of the boot modules */
		*(EXCLUDE_FILE (*boot_modules.o) .data.*)
	} : rw

	/* exception frames for C++ */
	.eh_frame : {
		__eh_frame_start__ = .;
		KEEP (*(.eh_frame))
		LONG(0)
	} : rw

	.gcc_except_table : {
		KEEP(*(.gcc_except_table))
		KEEP(*(.gcc_except_table.*))
	}

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
		_bss_start = ALIGN(8);
		*(.bss .bss.* .gnu.linkonce.b.* COMMON)
	}
	_bss_end = ALIGN(8);

	/* separate location for the binaries of the boot modules */
	.data.boot_modules_binaries : { *(.data.boot_modules_binaries) } : boot

	/* end of program image -- must be after last section */
	_prog_img_end = .;

	/DISCARD/ : {
		*(.note)
		*(.note.ABI-tag)
		*(.comment)
	}
}
