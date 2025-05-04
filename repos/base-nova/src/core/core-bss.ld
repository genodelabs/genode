SECTIONS
{
	.data : {
		*(.bss .bss.* .lbss .lbss.* .gnu.linkonce.b.* COMMON)
	} : rw
}
