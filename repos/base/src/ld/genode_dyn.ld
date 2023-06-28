/*
 * \brief  Linker script for dynamically-linked Genode programs
 * \author Sebastian Sumpf
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * The entry point of dynamically linked components is unused. The dynamic
 * linker determines the 'Component::construct' function via a symbol lookup.
 */

PHDRS
{
  interp   PT_INTERP;
  ro       PT_LOAD;
  rw       PT_LOAD;
  dynamic  PT_DYNAMIC;
  eh_frame PT_GNU_EH_FRAME;
}

SECTIONS
{
  /* Read-only sections, merged into text segment: */
  .text :
  {
    _prog_img_beg = .;

    /*
     * The ELF entry point is unused for dynamically linked components. The
     * dynamic linker determines the entry point by looking up the symbol of
     * the 'Component::construct' function.
     *
     * \deprecated  The support for legacy main functions will be removed.
     *
     * The 'KEEP' directive prevents text that is reachable from one of the
     * possible entry points from being garbage collected.
     */
    KEEP(*(.text._ZN9Component9constructERN6Genode3EnvE))

    *(.text
      .stub .text.* .gnu.linkonce.t.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  } : ro =0x0

  .interp         : { *(.interp) } : interp : ro
  .note.gnu.build-id : { *(.note.gnu.build-id) }
  .hash           : { *(.hash) }
  .gnu.hash       : { *(.gnu.hash) }
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }
  .gnu.version    : { *(.gnu.version) }
  .gnu.version_d  : { *(.gnu.version_d) }
  .gnu.version_r  : { *(.gnu.version_r) }
  .rel.init       : { *(.rel.init) }
  .rela.init      : { *(.rela.init) }
  .rel.text       : { *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*) }
  .rela.text      : { *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*) }
  .rel.fini       : { *(.rel.fini) }
  .rela.fini      : { *(.rela.fini) }
  .rel.rodata     : { *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*) }
  .rela.rodata    : { *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*) }
  .rel.data.rel.ro   : { *(.rel.data.rel.ro* .rel.gnu.linkonce.d.rel.ro.*) }
  .rela.data.rel.ro   : { *(.rela.data.rel.ro* .rela.gnu.linkonce.d.rel.ro.*) }
  .rel.data       : { *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*) }
  .rela.data      : { *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*) }
  .rel.tdata	  : { *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*) }
  .rela.tdata	  : { *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*) }
  .rel.tbss	  : { *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*) }
  .rela.tbss	  : { *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*) }
  .rel.ctors      : { *(.rel.ctors) }
  .rela.ctors     : { *(.rela.ctors) }
  .rel.dtors      : { *(.rel.dtors) }
  .rela.dtors     : { *(.rela.dtors) }
  .rel.got        : { *(.rel.got) }
  .rela.got       : { *(.rela.got) }
  .rel.bss        : { *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*) }
  .rela.bss       : { *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*) }
  .rel.plt        : { *(.rel.plt) }
  .rela.plt       : { *(.rela.plt) }
  .init           :
  {
    KEEP (*(.init))
  } = 0x0
  .plt            : { *(.plt) }

  .fini           :
  {
    KEEP (*(.fini))
  } = 0x0
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  .rodata         : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  .rodata1        : { *(.rodata1) }

  .eh_frame_hdr : { *(.eh_frame_hdr) } : eh_frame : ro

  /*
   * Adjust the address for the data segment.  We want to adjust up to
   * the same address within the page on the next page up.
   */
  . = ALIGN(0x1000);
  .root_cap : {
    /*
     * Leave space for parent capability parameters at start of data
     * section. The protection-domain creator is reponsible for storing
     * sane values here.
     */
     _parent_cap = .;
     LONG(0xffffffff);
     LONG(0xffffffff);
     LONG(0xffffffff);
     LONG(0xffffffff);
     LONG(0xffffffff);
     LONG(0xffffffff);
     LONG(0xffffffff);
     LONG(0xffffffff);
  } : rw

  /* Exception handling  */
  .eh_frame       :
  {
    __eh_frame_start__ = .;
    KEEP (*(.eh_frame))
    LONG(0);
   } : rw
  .gcc_except_table   : { *(.gcc_except_table .gcc_except_table.*) }

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

  /* Take out for now because these sections cause warnings on ARM. If this
   * should lead to an error then we'll take it back in.
  .preinit_array     :
  {
    __preinit_array_start = .;
    KEEP (*(.preinit_array))
    __preinit_array_end = .;
  }
  .init_array     :
  {
     __init_array_start = .;
     KEEP (*(SORT(.init_array.*)))
     KEEP (*(.init_array))
     __init_array_end = .;
  }
  .fini_array     :
  {
    __fini_array_start = .;
    KEEP (*(.fini_array))
    KEEP (*(SORT(.fini_array.*)))
    __fini_array_end = .;
  } */
  .ctors          :
  {
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */

    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
       the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
    _ctors_start = .;
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array))
    _ctors_end = .;
  }
  .dtors          :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    _dtors_start = .;
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    KEEP (*(.fini_array))
    KEEP (*(SORT(.fini_array.*)))
    _dtors_end = .;
  }
  .jcr            : { KEEP (*(.jcr)) }
  .data.rel.ro : { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro* .gnu.linkonce.d.rel.ro.*) }

  /* merge .got.plt and .got into .got, since the ARM toolchain for OKL4
   * set's * DT_PLTGOT to .got instead of .got.plt */
  .got            : { *(.got.plt) *(.got) }

  .data           :
  {
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

    __dso_handle = .;
    LONG(0x0);
  }
  .data1          : { *(.data1) }

  .dynamic        :
  {
    *(.dynamic)

    /*
     * Make sure to have a least one entry within the dynamic section, so the
     * dynamic segment points to something valid in case there are no dynamic
     * symbols in a binary. These semi-static binaries are not supported by
     * our dynamic linker that depends on the .dynamic section.
     */
    LONG(0x0);
   } : rw : dynamic

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
  } : rw
  . = ALIGN(32 / 8);
  . = ALIGN(32 / 8);
  _end = .; PROVIDE (end = .);

  /* end of program image -- must be after last section */
  _prog_img_end = .;

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
