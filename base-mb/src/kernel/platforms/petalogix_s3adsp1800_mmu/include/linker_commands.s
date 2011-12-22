.macro _BEGIN_ELF_ENTRY_CODE
	.global _start
	.section ".Elf_entry"

	_start:
.endm


.macro _BEGIN_ATOMIC_OPS
	.section ".Atomic_ops"
	.global _atomic_ops_begin
	.align 12
	_atomic_ops_begin:
.endm


.macro _END_ATOMIC_OPS
	.global _atomic_ops_end
	.align 12
	_atomic_ops_end:
.endm


.macro _BEGIN_READABLE_EXECUTABLE
	.section ".text"
.endm


.macro _BEGIN_READABLE_WRITEABLE
	.section ".bss"
.endm



