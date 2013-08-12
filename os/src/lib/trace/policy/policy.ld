PHDRS { rw PT_LOAD; }
SECTIONS {

	.text : {
		*(.data)  /* must be first because it contains policy module header */
		*(.text .text.*)
		*(.bss)
	} : rw

	/DISCARD/ : { *(.*) }
}
