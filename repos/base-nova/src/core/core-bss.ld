SECTIONS
{
	.data : {
		*(.bss .bss.* .gnu.linkonce.b.* COMMON)
	} : rw
}
