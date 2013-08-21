PHDRS { rw PT_LOAD; }
SECTIONS {

	.text : {
		*(.data.rel) /* must be first because it contains policy module header */
		*(.data)
		*(.rodata .rodata.*)
		*(.text .text.*)
		*(.bss)
		*(.got.plt)
	} : rw

	/DISCARD/ : { *(.*) }
}
