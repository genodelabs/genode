
.section .text

	.balign 4096
	.global bogomips
	bogomips:
	mov x9, x0
	1:
	cbz x9, 2f
	sub x9, x9, #1
	.rept 1
	nop
	.endr
	b 1b
	2:
	ret

	.global bogomips_instr_count
	bogomips_instr_count:
	adr x9,  1b
	adr x10, 2b
	sub x0, x10, x9
	lsr x0, x0, #2
	ret
