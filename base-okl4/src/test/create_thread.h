/*
 * \brief  Thread creation on OKL4
 * \author Norman Feske
 * \date   2009-03-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CREATE_THREAD_H_
#define _CREATE_THREAD_H_

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/types.h>
#include <l4/utcb.h>
#include <l4/config.h>
#include <l4/thread.h>
#include <l4/ipc.h>
#include <l4/message.h>
#include <l4/schedule.h>
} }

namespace Okl4 {
	extern L4_Word_t copy_uregister_to_utcb(void);
}

/* Genode includes */
#include <base/printf.h>
#include <base/native_types.h>


enum { DEFAULT_PRIORITY = 100 };

/**
 * Create and start new thread
 *
 * \param thread_no  designated thread number of new thread
 * \param space_no   space ID in which the new thread will be executed
 * \param sp         initial stack pointer
 * \param ip         initial instruction pointer
 * \return           native thread ID
 */
static inline Okl4::L4_ThreadId_t create_thread(int thread_no, int space_no,
                                                void *sp, void (*ip)(void))
{
	using namespace Okl4;

	/* activate local thread by assigning a UTCB address and thread ID */
	L4_ThreadId_t new_thread_id     = L4_GlobalId(thread_no, 1);
	L4_SpaceId_t  roottask_space_id = L4_SpaceId(space_no);
	L4_ThreadId_t scheduler         = L4_rootserver;
	L4_ThreadId_t pager             = L4_rootserver;
	L4_ThreadId_t exception_handler = L4_rootserver;
	L4_Word_t     resources         = 0;
	L4_Word_t     utcb_location     = (L4_Word_t)utcb_base_get()
	                                + space_no*L4_GetUtcbAreaSize()
	                                + thread_no*L4_GetUtcbSize();
#ifdef NO_UTCB_RELOCATE
	utcb_location = ~0;  /* UTCB allocation is handled by the kernel */
#endif

	int ret = L4_ThreadControl(new_thread_id,
	                           roottask_space_id,
	                           scheduler, pager, exception_handler,
	                           resources, (void *)utcb_location);
	if (ret != 1) {
		PERR("L4_ThreadControl returned %d, error code=%d",
		     ret, (int)L4_ErrorCode());
		for (;;);
		return L4_nilthread;
	}

	/* let the new thread know its global thread id */
	L4_Set_UserDefinedHandleOf(new_thread_id, new_thread_id.raw);

	/* start thread */
	L4_Start_SpIp(new_thread_id, (L4_Word_t)sp, (L4_Word_t)ip);

	/* set default priority */
	L4_Set_Priority(new_thread_id, DEFAULT_PRIORITY);

	return new_thread_id;
}


/**
 * Perform thread startup protocol to make global ID known to the ourself
 *
 * This function must be executed by a newly created thread to make
 * 'thread_get_my_global_id' to work.
 */
static inline void thread_init_myself()
{
	using namespace Okl4;

	/*
	 * Read global thread ID from user-defined handle and store it
	 * into a designated UTCB entry.
	 */
	L4_Word_t my_global_id = L4_UserDefinedHandle();
	__L4_TCR_Set_ThreadWord(Genode::UTCB_TCR_THREAD_WORD_MYSELF, my_global_id);
}


/**
 * Register the rootserver's thread ID at our UTCB
 *
 * This function must be executed at the startup of the rootserver main
 * thread to make 'thread_get_my_gloal_id' to work.
 */
static inline void roottask_init_myself()
{
	using namespace Okl4;

	/*
	 * On Genode, the user-defined handle get initialized with the thread's
	 * global ID by core when creating the new thread. For the main thread,
	 * we do this manually.
	 */
	__L4_TCR_Set_UserDefinedHandle(L4_rootserver.raw);
	copy_uregister_to_utcb();
}


/**
 * Return the global thread ID of the calling thread
 *
 * On OKL4 we cannot use 'L4_Myself()' to determine our own thread's
 * identity. By convention, each thread stores its global ID in a
 * defined entry of its UTCB.
 */
static inline Okl4::L4_ThreadId_t thread_get_my_global_id()
{
	Okl4::L4_ThreadId_t myself;
	myself.raw = Okl4::__L4_TCR_ThreadWord(Genode::UTCB_TCR_THREAD_WORD_MYSELF);
	return myself;
}

#endif /* _CREATE_THREAD_H_ */
