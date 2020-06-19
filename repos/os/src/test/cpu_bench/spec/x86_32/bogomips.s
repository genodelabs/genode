.section .text

	.balign 4096
	.global bogomips
	bogomips:
	push %ebp
	mov  %esp, %ebp
	mov  0x8(%ebp), %eax
	1:
	cmp  $0x0, %eax
	je 2f
	sub  $0x1, %eax
	.rept 1
	nop
	.endr
	jmp  1b
	2:
	mov  %ebp, %esp
	pop  %ebp
	ret

	.global bogomips_instr_count
	bogomips_instr_count:
	mov $0x5, %eax
	ret

