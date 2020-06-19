
.section .text

	.balign 4096
	.global bogomips
	bogomips:
	push { r4, lr }
	mov r4, #0
	1:
	cmp r4, r0
	addne r4, r4, #1
	.rept 1
	nop
	.endr
	bne 1b
	2:
	pop  { r4, lr }
	mov pc, lr

	.global bogomips_instr_count
	bogomips_instr_count:
	push { r4, r5, lr }
	adr r4, 1b
	adr r5, 2b
	sub r0, r5, r4
	lsr r0, r0, #2
	pop  { r4, r5, lr }
	mov pc, lr
