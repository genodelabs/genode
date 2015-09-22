/*
 * \brief  Linux syscall() binding
 * \author Christian Prochaska
 * \date   2009-07-14
 *
 * based on glibc-2.9/sysdeps/unix/sysv/linux/i386/syscall.S
 *
 * error case:
 *   glibc's syscall() function returns -1 and sets errno
 *   lx_syscall() returns -errno
 */


#define L(name) name

#define ENTER_KERNEL int $0x80

	.text
	.globl lx_syscall
	.type lx_syscall, @function
lx_syscall:
    .cfi_startproc
/*    PUSHARGS_6*/      /* Save register contents. */
/* PUSHARGS_6 begin */
    pushl %ebp;
    .cfi_adjust_cfa_offset 4;
    .cfi_rel_offset %ebp, 0;
L(PUSHBP1):
    pushl %edi;
    .cfi_adjust_cfa_offset 4;
    .cfi_rel_offset %edi, 0;
L(PUSHDI1):
    pushl %esi;
    .cfi_adjust_cfa_offset 4;
    .cfi_rel_offset %esi, 0;
L(PUSHSI1):
    pushl %ebx;
    .cfi_adjust_cfa_offset 4;
    .cfi_rel_offset %ebx, 0;
L(PUSHBX1):
/* PUSHARGS_6 end */

    /*_DOARGS_6(44)*/       /* Load arguments.  */
/*_DOARGS_6(44) begin*/
    movl 44(%esp), %ebp;
    movl 40(%esp), %edi;
    movl 36(%esp), %esi;
    movl 32(%esp), %edx;
    movl 28(%esp), %ecx;
    movl 24(%esp), %ebx;
/*_DOARGS_6(44) end*/
    movl 20(%esp), %eax /* Load syscall number into %eax.  */
    ENTER_KERNEL        /* Do the system call.  */
/*    POPARGS_6*/       /* Restore register contents.  */
/*    POPARGS_6 begin */
    popl %ebx;
    .cfi_adjust_cfa_offset -4;
    .cfi_restore %ebx;
L(POPBX1):
    popl %esi;
    .cfi_adjust_cfa_offset -4;
    .cfi_restore %esi;
L(POPSI1):
    popl %edi;
    .cfi_adjust_cfa_offset -4;
    .cfi_restore %edi;
L(POPDI1):
    popl %ebp;
    .cfi_adjust_cfa_offset -4;
    .cfi_restore %ebp;
L(POPBP1):
/*    POPARGS_6 end */
L(pseudo_end):
    ret         /* Return to caller.  */

    .cfi_endproc

/*
 * Allow stacks to be mapped executable (needed because Genode does not
 * offer an API to handle non-execute mappings yet).
 */
.section .note.GNU-stack, "", @progbits

