.section .text

	.balign 4096
	.global bogomips
	bogomips:
	push %rdi
	1:
	cmp  $0x0, %rdi
	je   2f
	sub  $0x1, %rdi
	.rept 1
	nop
	.endr
	jmp  1b
	2:
	pop  %rdi
	ret

	.global bogomips_instr_count
	bogomips_instr_count:
	mov $0x5, %rax
	ret

