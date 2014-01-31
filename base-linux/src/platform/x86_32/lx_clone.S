/*
 * \brief  Linux clone() binding
 * \author Christian Prochaska
 * \date   2009-07-14
 *
 * based on glibc-2.9/sysdeps/unix/sysv/linux/i386/clone.S
 */


#define L(name) name

#define ENTER_KERNEL int $0x80
#define SYS_clone 120
#define SYS_exit 1

#define LINKAGE 4
#define PTR_SIZE 4

#define PARMS   LINKAGE     /* no space for saved regs */
#define FUNC    PARMS
#define STACK   FUNC+4
#define FLAGS   STACK+PTR_SIZE
#define ARG FLAGS+4
#define PTID    ARG+PTR_SIZE
#define TLS PTID+PTR_SIZE
#define CTID    TLS+PTR_SIZE

    .text
    .globl lx_clone
    .type lx_clone, @function
lx_clone:
    .cfi_startproc

    /* Insert the argument onto the new stack.  Make sure the new
       thread is started with an alignment of (mod 16).  */
    movl    STACK(%esp),%ecx
    andl    $0xfffffff0, %ecx
    subl    $28,%ecx
    movl    ARG(%esp),%eax      /* no negative argument counts */
    movl    %eax,12(%ecx)

    /* Save the function pointer as the zeroth argument.
       It will be popped off in the child in the ebx frobbing below.  */
    movl    FUNC(%esp),%eax
    movl    %eax,8(%ecx)
    /* Don't leak any information.  */
    movl    $0,4(%ecx)
    movl    $0,(%ecx)

    /* Do the system call */
    pushl   %ebx
    .cfi_adjust_cfa_offset (4)
    pushl   %esi
    .cfi_adjust_cfa_offset (4)
    pushl   %edi
    .cfi_adjust_cfa_offset (4)

    movl    TLS+12(%esp),%esi
    .cfi_rel_offset %esi, 4
    movl    PTID+12(%esp),%edx
    movl    FLAGS+12(%esp),%ebx
    .cfi_rel_offset %ebx, 8
    movl    CTID+12(%esp),%edi
    .cfi_rel_offset %edi, 0
    movl    $SYS_clone,%eax

    /* End FDE now, because in the child the unwind info will be
       wrong.  */
    .cfi_endproc

    ENTER_KERNEL
    popl    %edi
    popl    %esi
    popl    %ebx

    test    %eax,%eax
    jz  L(thread_start)

L(pseudo_end):
    ret

L(thread_start):
    .cfi_startproc;
    /* Clearing frame pointer is insufficient, use CFI.  */
    .cfi_undefined %eip;
    xorl    %ebp,%ebp   /* terminate the stack frame */
    call    *%ebx
#ifdef PIC
    call    L(here)
L(here):
    popl    %ebx
    addl    $_GLOBAL_OFFSET_TABLE_+[.-L(here)], %ebx
#endif
    movl    %eax, %ebx
    movl    $SYS_exit, %eax
    ENTER_KERNEL

    .cfi_endproc;

    .cfi_startproc
    .cfi_endproc

/*
 * Allow stacks to be mapped executable (needed because Genode does not
 * offer an API to handle non-execute mappings yet).
 */
.section .note.GNU-stack, "", @progbits

