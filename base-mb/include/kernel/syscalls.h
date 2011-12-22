/*
 * \brief  Kernels syscall frontend
 * \author Martin stein
 * \date   2010.07.02
 */

#ifndef _INCLUDE__KERNEL__SYSCALLS_H_
#define _INCLUDE__KERNEL__SYSCALLS_H_

/* Kernel includes */
#include <kernel/types.h>
#include <cpu/config.h>

/**
 * Inline assembly clobber lists for syscalls with no arguments
 */
#define SYSCALL_7_ASM_CLOBBER "r24", SYSCALL_6_ASM_CLOBBER
#define SYSCALL_6_ASM_CLOBBER "r25", SYSCALL_5_ASM_CLOBBER
#define SYSCALL_5_ASM_CLOBBER "r26", SYSCALL_4_ASM_CLOBBER
#define SYSCALL_4_ASM_CLOBBER "r27", SYSCALL_3_ASM_CLOBBER
#define SYSCALL_3_ASM_CLOBBER "r28", SYSCALL_2_ASM_CLOBBER
#define SYSCALL_2_ASM_CLOBBER "r29", SYSCALL_1_ASM_CLOBBER
#define SYSCALL_1_ASM_CLOBBER        SYSCALL_0_ASM_CLOBBER
#define SYSCALL_0_ASM_CLOBBER "r31", "r30"

/**
 * Inline assembly list for write access during syscalls with no arguments
 */
#define SYSCALL_0_ASM_WRITE \
	[result]  "=m" (result), \
	[r15_buf] "+m" (r15_buf), \
	[opcode]  "+m" (opcode)


/**
 * Inline assembly lists for write access during syscalls with arguments
 */
#define SYSCALL_1_ASM_WRITE [arg_0] "+m" (arg_0), SYSCALL_0_ASM_WRITE
#define SYSCALL_2_ASM_WRITE [arg_1] "+m" (arg_1), SYSCALL_1_ASM_WRITE
#define SYSCALL_3_ASM_WRITE [arg_2] "+m" (arg_2), SYSCALL_2_ASM_WRITE
#define SYSCALL_4_ASM_WRITE [arg_3] "+m" (arg_3), SYSCALL_3_ASM_WRITE
#define SYSCALL_5_ASM_WRITE [arg_4] "+m" (arg_4), SYSCALL_4_ASM_WRITE
#define SYSCALL_6_ASM_WRITE [arg_5] "+m" (arg_5), SYSCALL_5_ASM_WRITE
#define SYSCALL_7_ASM_WRITE [arg_6] "+m" (arg_6), SYSCALL_6_ASM_WRITE

/**
 * Inline assembly ops for syscalls with no arguments
 * - r19-r31 are save when occuring in the clobber list
 *   r15 is a 'dedicated' register and so we have to save it manually
 */
#define SYSCALL_0_ASM_OPS \
	"lwi r31, %[opcode]  \n" \
	"swi r15, %[r15_buf] \n" \
	"brki r15, 0x8       \n" \
	"or r0, r0, r0       \n" \
	"lwi r15, %[r15_buf] \n" \
	"swi r30, %[result]    " 

/**
 * Inline assembly ops for syscalls with arguments
 */
#define SYSCALL_1_ASM_OPS "lwi r30, %[arg_0]\n" SYSCALL_0_ASM_OPS
#define SYSCALL_2_ASM_OPS "lwi r29, %[arg_1]\n" SYSCALL_1_ASM_OPS
#define SYSCALL_3_ASM_OPS "lwi r28, %[arg_2]\n" SYSCALL_2_ASM_OPS
#define SYSCALL_4_ASM_OPS "lwi r27, %[arg_3]\n" SYSCALL_3_ASM_OPS
#define SYSCALL_5_ASM_OPS "lwi r26, %[arg_4]\n" SYSCALL_4_ASM_OPS
#define SYSCALL_6_ASM_OPS "lwi r25, %[arg_5]\n" SYSCALL_5_ASM_OPS
#define SYSCALL_7_ASM_OPS "lwi r24, %[arg_6]\n" SYSCALL_6_ASM_OPS

/**
 * Inline assembly lists for read access during syscalls with arguments
 */
#define SYSCALL_0_ASM_READ  
#define SYSCALL_1_ASM_READ SYSCALL_0_ASM_READ
#define SYSCALL_2_ASM_READ SYSCALL_1_ASM_READ
#define SYSCALL_3_ASM_READ SYSCALL_2_ASM_READ
#define SYSCALL_4_ASM_READ SYSCALL_3_ASM_READ
#define SYSCALL_5_ASM_READ SYSCALL_4_ASM_READ
#define SYSCALL_6_ASM_READ SYSCALL_5_ASM_READ
#define SYSCALL_7_ASM_READ SYSCALL_6_ASM_READ


namespace Kernel {

	using namespace Cpu;

	typedef unsigned int Syscall_arg;

	/** 
	 * Syscall with 1 Argument 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode);


	/**
	 * Syscall with 2 Arguments 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode,
	                                 Syscall_arg arg_0);

	/**
	 * Syscall with 3 Arguments 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode,
	                                 Syscall_arg arg_0,
	                                 Syscall_arg arg_1);

	/**
	 * Syscall with 4 Arguments 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode,
	                                 Syscall_arg arg_0,
	                                 Syscall_arg arg_1,
	                                 Syscall_arg arg_2);

	/**
	 * Syscall with 5 Arguments 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode,
	                                 Syscall_arg arg_0,
	                                 Syscall_arg arg_1,
	                                 Syscall_arg arg_2,
	                                 Syscall_arg arg_3);

	/**
	 * Syscall with 6 Arguments 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode,
	                                 Syscall_arg arg_0,
	                                 Syscall_arg arg_1,
	                                 Syscall_arg arg_2,
	                                 Syscall_arg arg_3,
	                                 Syscall_arg arg_4);

	/**
	 * Syscall with 7 Arguments 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode,
	                                 Syscall_arg arg_0,
	                                 Syscall_arg arg_1,
	                                 Syscall_arg arg_2,
	                                 Syscall_arg arg_3,
	                                 Syscall_arg arg_4,
	                                 Syscall_arg arg_5);

	/**
	 * Syscall with 8 Arguments 
	 */
	ALWAYS_INLINE inline int syscall(Syscall_id opcode,
	                                 Syscall_arg arg_0,
	                                 Syscall_arg arg_1,
	                                 Syscall_arg arg_2,
	                                 Syscall_arg arg_3,
	                                 Syscall_arg arg_4,
	                                 Syscall_arg arg_5,
	                                 Syscall_arg arg_6);

	/**
	 * Yield thread execution and coninue with next
	 */
	inline void thread_yield();

	/**
	 * Block thread that calls this
	 */
	inline void thread_sleep();

	/**
	 * Create and start threads
	 *
	 * \param  tid  ident that thread should get
	 * \param  pid  threads protection domain
	 * \param  pager_id  threads page fault handler thread
	 * \param  utcb_p  virtual address of utcb
	 * \param  vip  initial virtual ip
	 * \param  vsp  initial virtual sp
	 * \param  param  scheduling parameters, not used by now
	 * \return  0 if new thread was created
	 *          n > 0 if any error has occured (errorcodes planned)
	 */
	inline int  thread_create(Thread_id      tid,
	                          Protection_id  pid,
	                          Thread_id      pager_tid,
	                          Utcb*          utcb_p,
	                          Cpu::addr_t    vip,
	                          Cpu::addr_t    vsp,
	                          unsigned int   params);

	/**
	 * Kill thread - only with root rights
	 *
	 * \param  tid  ident of thread
	 * \return  0 if thread is awake after syscall
	 *          n > 0 if any error has occured (errorcodes planned)
	 */
	inline int thread_kill(Thread_id tid);

	/**
	 * Unblock denoted thread
	 *
	 * \param  tid  ident of thread thats blocked
	 * \detail  works only if destination has same protection
	 *          domain or caller has rootrights
	 * \return  0 if thread is awake after syscall
	 *          n > 0 if any error has occured (errorcodes planned)
	 */
	inline int thread_wake(Thread_id tid);

	/**
	 * Re-set pager of another thread
	 *
	 * \param  dst_tid  thread whose pager shall be changed
	 * \param  pager_tid  ident of pager thread
	 * \detail  works only if caller has rootrights
	 * \return  0 if new pager of thread is successfully set
	 *          n > 0 if any error has occured (errorcodes planned)
	 */
	inline int thread_pager(Thread_id dst_tid,
	                        Thread_id pager_tid);

	/**
	 * Reply last and wait for new ipc request
	 *
	 * \param  msg_length  length of reply message
	 * \return  length of received message
	 */
	inline int ipc_serve(unsigned int reply_size);

	/**
	 * Send ipc request denoted in utcb to specific thread
	 *
	 * \param  dest_id     ident of destination thread
	 * \param  msg_length  number of request-message words
	 * \return  number of reply-message words, or
	 *          zero if request was not successfull
	 */
	inline int ipc_request(Thread_id    dest_tid, unsigned int msg_size);

	/**
	 * Load pageresolution to memory managment unit
	 *
	 * \param  p_addr  physical page address
	 * \param  v_addr  virtual page address
	 * \param  pid  protection domain ident
	 * \param  size  size of page
	 * \param  permissions  permission flags for page
	 * \return  0 if thread is awake after syscall
	 *          n > 0 if any error has occured (errorcodes planned)
	 */
	inline int tlb_load(Cpu::addr_t                        p_address,
	                    Cpu::addr_t                        v_address,
	                    Protection_id                      pid,
	                    Paging::Physical_page::size_t      size,
	                    Paging::Physical_page::Permissions permissions);

	/**
	 * Flush page resolution area from tlb
	 *
	 * \param  pid  protection domain id
	 * \param  start  startaddress of area
	 * \param  size_kbyte  size of area in 1KB units
	 * \return  0 if new thread was created
	 *          n > 0 if any error has occured (errorcodes planned)
	 */
	inline int tlb_flush(Protection_id pid,
	                     Cpu::addr_t   start,
	                     unsigned      size);

	/**
	 * Print char to serial ouput
	 *
	 * \param  c  char to print
	 */
	inline void print_char(char c);

	/**
	 * Print various informations about a specific thread
	 * \param  i  Unique ID of the thread, if it remains 0 take our own ID
	 */
	inline void print_info(Thread_id const & i = 0);

	/**
	 * Allocate an IRQ to the calling thread if the IRQ is
	 * not allocated yet to another thread
	 *
	 * \param   i       Unique ID of the IRQ
	 * \return  0       If the IRQ is allocated to this thread now
	 *          n != 0  If the IRQ is not allocated to this thread already
	 *                  (code of the error that has occured)
	 */
	inline int irq_allocate(Irq_id i);

	/**
	 * Free an IRQ from allocation if it is allocated by the
	 * calling thread
	 *
	 * \param   i       Unique ID of the IRQ
	 * \return  0       If the IRQ is free now
	 *          n != 0  If the IRQ is allocated already
	 *                  (code of the error that has occured)
	 */
	inline int irq_free(Irq_id i);

	/**
	 * Sleep till the 'Irq_message'-queue of this thread is not
	 * empty. For any IRQ that is allocated by this thread and occures
	 * between the kernel-entrance inside 'irq_wait' and the next time this
	 * thread wakes up, an 'Irq_message' with metadata about the according
	 * IRQ is added to the threads 'Irq_message'-queue.
	 * When returning from 'irq_wait' the first message from the threads
	 * 'Irq_message'-queue is dequeued and written to the threads UTCB-base.
	 */
	inline void irq_wait();
}


void Kernel::print_info(Thread_id const & i)
{
	syscall(PRINT_INFO, (Syscall_arg) i);
}


void Kernel::irq_wait() { syscall(IRQ_WAIT); }


int Kernel::irq_allocate(Irq_id i)
{
	return syscall(IRQ_ALLOCATE, (Syscall_arg) i);
}


int Kernel::irq_free(Irq_id i) { return syscall(IRQ_FREE, (Syscall_arg) i); }


void Kernel::thread_yield() { syscall(THREAD_YIELD); }


void Kernel::thread_sleep() { syscall(THREAD_SLEEP); }


int Kernel::thread_create(Thread_id      tid,
                          Protection_id  pid,
                          Thread_id      pager_tid,
                          Utcb*          utcb_p,
                          Cpu::addr_t    vip,
                          Cpu::addr_t    vsp,
                          unsigned int   params)
{
	return syscall(THREAD_CREATE,
	               (Syscall_arg) tid,
	               (Syscall_arg) pid,
	               (Syscall_arg) pager_tid,
	               (Syscall_arg) utcb_p,
	               (Syscall_arg) vip,
	               (Syscall_arg) vsp,
	               (Syscall_arg) params);
}


int Kernel::thread_kill(Thread_id tid)
{
	return syscall(THREAD_KILL, (Syscall_arg) tid);
}


int Kernel::thread_wake(Thread_id tid)
{
	return syscall(THREAD_WAKE, (Syscall_arg) tid);
}


int Kernel::thread_pager(Thread_id dst_tid,
                         Thread_id pager_tid)
{
	return syscall(
		THREAD_PAGER,
		(Syscall_arg) dst_tid,
		(Syscall_arg) pager_tid);
}


int Kernel::ipc_serve(unsigned int reply_size)
{
	return  syscall(IPC_SERVE, (Syscall_arg) reply_size);
}


int Kernel::ipc_request(Thread_id    dest_tid,
                        unsigned int msg_size)
{
	return syscall(
		IPC_REQUEST,
		(Syscall_arg) dest_tid,
		(Syscall_arg) msg_size);
}


int Kernel::tlb_load(Cpu::addr_t                        p_address,
                     Cpu::addr_t                        v_address,
                     Protection_id                      pid,
                     Paging::Physical_page::size_t      size,
                     Paging::Physical_page::Permissions permissions)
{
	return syscall(
		TLB_LOAD,
		(Syscall_arg) p_address,
		(Syscall_arg) v_address,
		(Syscall_arg) pid,
		(Syscall_arg) size,
		(Syscall_arg) permissions);
}


int Kernel::tlb_flush(Protection_id pid,
                      Cpu::addr_t   start,
                      unsigned size)
{
	return syscall(
		TLB_FLUSH,
		(Syscall_arg) pid,
		(Syscall_arg) start,
		(Syscall_arg) size);
}


void Kernel::print_char(char c) { syscall(PRINT_CHAR, (Syscall_arg) c); }


int Kernel::syscall(Syscall_id opcode)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_0_ASM_OPS
	           : SYSCALL_0_ASM_WRITE
	           : SYSCALL_0_ASM_READ
	           : SYSCALL_0_ASM_CLOBBER);

	return result;
}


int Kernel::syscall(Syscall_id opcode, Syscall_arg arg_0)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_1_ASM_OPS
	           : SYSCALL_1_ASM_WRITE
	           : SYSCALL_1_ASM_READ
	           : SYSCALL_1_ASM_CLOBBER);

	return result;
}


int Kernel::syscall(Syscall_id opcode,
                    Syscall_arg arg_0,
                    Syscall_arg arg_1)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_2_ASM_OPS
	           : SYSCALL_2_ASM_WRITE
	           : SYSCALL_2_ASM_READ
	           : SYSCALL_2_ASM_CLOBBER);

	return result;
}


int Kernel::syscall(Syscall_id opcode,
                    Syscall_arg arg_0,
                    Syscall_arg arg_1,
                    Syscall_arg arg_2)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_3_ASM_OPS
	           : SYSCALL_3_ASM_WRITE
	           : SYSCALL_3_ASM_READ
	           : SYSCALL_3_ASM_CLOBBER);

	return result;
}


int Kernel::syscall(Syscall_id opcode,
                    Syscall_arg arg_0,
                    Syscall_arg arg_1,
                    Syscall_arg arg_2,
                    Syscall_arg arg_3)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_4_ASM_OPS
	           : SYSCALL_4_ASM_WRITE
	           : SYSCALL_4_ASM_READ
	           : SYSCALL_4_ASM_CLOBBER);

	return result;
}


int Kernel::syscall(Syscall_id opcode,
                    Syscall_arg arg_0,
                    Syscall_arg arg_1,
                    Syscall_arg arg_2,
                    Syscall_arg arg_3,
                    Syscall_arg arg_4)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_5_ASM_OPS
	           : SYSCALL_5_ASM_WRITE
	           : SYSCALL_5_ASM_READ
	           : SYSCALL_5_ASM_CLOBBER);

	return result;
}


int Kernel::syscall(Syscall_id opcode,
                    Syscall_arg arg_0,
                    Syscall_arg arg_1,
                    Syscall_arg arg_2,
                    Syscall_arg arg_3,
                    Syscall_arg arg_4,
                    Syscall_arg arg_5)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_6_ASM_OPS
	           : SYSCALL_6_ASM_WRITE
	           : SYSCALL_6_ASM_READ
	           : SYSCALL_6_ASM_CLOBBER);

	return result;
}


int Kernel::syscall(Syscall_id opcode,
                    Syscall_arg arg_0,
                    Syscall_arg arg_1,
                    Syscall_arg arg_2,
                    Syscall_arg arg_3,
                    Syscall_arg arg_4,
                    Syscall_arg arg_5,
                    Syscall_arg arg_6)
{
	int result;
	unsigned int r15_buf;

	asm volatile(SYSCALL_7_ASM_OPS
	           : SYSCALL_7_ASM_WRITE
	           : SYSCALL_7_ASM_READ
	           : SYSCALL_7_ASM_CLOBBER);

	return result;
}


#endif /* _INCLUDE__KERNEL__SYSCALLS_H_ */
