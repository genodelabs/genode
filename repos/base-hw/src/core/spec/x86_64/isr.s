.data
.global _isr_array
_isr_array:

.text

.macro _isr_entry
	.align 4, 0x90
1:	.data
	.quad 1b
	.previous
.endm

.macro _exception_with_code vector
	_isr_entry
	push $\vector
	jmp _mt_kernel_entry_pic
.endm

.macro _exception vector
	_isr_entry
	push $0
	push $\vector
	jmp _mt_kernel_entry_pic
.endm

_exception				0
_exception				1
_exception				2
_exception				3
_exception				4
_exception				5
_exception				6
_exception				7
_exception_with_code 	8
_exception				9
_exception_with_code	10
_exception_with_code	11
_exception_with_code	12
_exception_with_code	13
_exception_with_code	14
_exception				15
_exception				16
_exception_with_code	17
_exception				18
_exception				19

.set vec, 20
.rept 236
_exception				vec
.set vec, vec + 1
.endr
