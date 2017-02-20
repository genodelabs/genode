/*
 * \brief  Linker script for libraries
 * \author Sebastian Sumpf
 * \date   2009-11-05
 *
 * Script for --shared -z combreloc: shared library, combine & sort relocs
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

PHDRS
{
  ro         PT_LOAD;
  rw         PT_LOAD;
  dynamic    PT_DYNAMIC;
  eh_frame   PT_GNU_EH_FRAME;
}

SECTIONS
{
  /* Read-only sections, merged into text segment: */
  .note.gnu.build-id : { *(.note.gnu.build-id) } : ro
  .hash           : { *(.hash) }
  .gnu.hash       : { *(.gnu.hash) }
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }
  .gnu.version    : { *(.gnu.version) }
  .gnu.version_d  : { *(.gnu.version_d) }
  .gnu.version_r  : { *(.gnu.version_r) }
  .rel.dyn        :
    {
      *(.rel.init)
      *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
      *(.rel.fini)
      *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
      *(.rel.data.rel.ro* .rel.gnu.linkonce.d.rel.ro.*)
      *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
      *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
      *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
      *(.rel.ctors)
      *(.rel.dtors)
      *(.rel.got)
      *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
    }
  .rela.dyn       :
    {
      *(.rela.init)
      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
      *(.rela.fini)
      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
      *(.rela.ctors)
      *(.rela.dtors)
      *(.rela.got)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
    }
  .rel.plt        : { *(.rel.plt) }
  .rela.plt       : { *(.rela.plt) }
  .init           :
  {
    KEEP (*(.init))
  } = 0x0
  .plt            : { *(.plt) }
  .text           :
  {
    *(.text .stub .text.* .gnu.linkonce.t.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  } = 0x0
  .fini           :
  {
    KEEP (*(.fini))
  } =0x0
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  .rodata         : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  .rodata1        : { *(.rodata1) }

  .eh_frame_hdr : { *(.eh_frame_hdr) } : eh_frame : ro
  .gcc_except_table   : { *(.gcc_except_table .gcc_except_table.*) }

  /*
   * Adjust the address for the data segment.  We want to adjust up to
   * the same address within the page on the next page up.
   */
  . = ALIGN(0x1000);

  .data           :
  {
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

    *(.data .data.* .gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
  } : rw

  /* .ARM.exidx is sorted, so has to go in its own output section */
  .ARM.extab : {
    *(.ARM.extab*)
  }
  __exidx_start = .;
  .ARM.exidx : {
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
  }
  __exidx_end = .;

  /* Thread Local Storage sections  */
  .tdata : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
  .tbss  : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }

  .preinit_array     :
  {
    KEEP (*(.preinit_array))
  }
  .fini_array     :
  {
    KEEP (*(.fini_array))
    KEEP (*(SORT(.fini_array.*)))
  }
  .ctors          :
  {
    /*
     * gcc uses crtbegin.o to find the start of the constructors, so we make
     * sure it is first.  Because this is a wildcard, it doesn't matter if the
     * user does not actually link against crtbegin.o; the linker won't look
     * for a file to match a wildcard.  The wildcard also means that it
     * doesn't matter which directory crtbegin.o is in.
     * */
  /*  KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))*/
    /*
     * We don't want to include the .ctor section from the crtend.o file until
     * after the sorted ctors.  The .ctor section from the crtend file contains
     * the end of ctors marker and it must be last
     */
    KEEP(*(_mark_ctors_start));
    _ctors_start = .;
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    KEEP (*(.init_array)) /* list of constructors specific for ARM eabi */
    _ctors_end = .;
    KEEP(*(_mark_ctors_end));
    
  }
  .dtors          :
  {
    PROVIDE(_dtors_start = .);
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    PROVIDE(_dtors_end = .);
  }
  .jcr            : { KEEP (*(.jcr)) }
  .data.rel.ro : { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro* .gnu.linkonce.d.rel.ro.*) } 

  .data1          : { *(.data1) }
  .dynamic        : { *(.dynamic) } : dynamic : rw
  /* See: genode_dyn.ld */
  .got            : { *(.got.plt) *(.got) }
  /* Exception handling  */
  .eh_frame :
  {
  	__eh_frame_start__ = .;
    KEEP (*(.eh_frame))
    LONG(0)
  }
  _edata = .; PROVIDE (edata = .);
  __bss_start = .;
  .bss            :
  {
   *(.dynbss)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)

   /*
    * Align here to ensure that the .bss section occupies space up to
    * _end.  Align after .bss to ensure correct alignment even if the
    * .bss section disappears because there are no input sections.
    *
    * FIXME: Why do we need it? When there is no .bss section, we don't
    * pad the .data section.
    */
   . = ALIGN(. != 0 ? 32 / 8 : 1);
  }
  . = ALIGN(32 / 8);
  . = ALIGN(32 / 8);
  _end = .; PROVIDE (end = .);
  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* DWARF 3 */
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges   0 : { *(.debug_ranges) }
  .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) }
}
