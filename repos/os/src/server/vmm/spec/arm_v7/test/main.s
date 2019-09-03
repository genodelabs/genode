.section ".text.crt0"

	.global _start
	_start:

	/* idle a little initially because U-Boot likes it this way */
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8

	/* zero-fill BSS segment */
	ldr r0, =_bss_start
	ldr r1, =_bss_end
	mov r2, #0
	1:
	cmp r1, r0
	ble 2f
	str r2, [r0]
	add r0, r0, #4
	b 1b
	2:

	hvc #0

	1: b 1b
