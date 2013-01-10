/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Norman Feske
 * \date   2009-04-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__IGUANA__THREAD_H_
#define _INCLUDE__OKLINUX_SUPPORT__IGUANA__THREAD_H_

#include <iguana/types.h>

/**
 * Initializes the Iguana library
 *
 * \param obj_env resulting pointer to the iguana environment object
 */
void __lib_iguana_init(uintptr_t *obj_env);

/**
 * Create a new thread in this address space
 *
 * \param thrd resulting pointer to the thread id of the new thread
 * \return     Iguana thread id of the new thread
 */
thread_ref_t thread_create(L4_ThreadId_t *thrd);

/**
 * Get OKL4 thread id from the Iguana thread id
 *
 * \param server Iguana thread id
 * \return       OKL4 thread id requested
 */
L4_ThreadId_t thread_l4tid(thread_ref_t server);

/**
 * Get the Iguana thread id of the active thread
 *
 * \return Iguana thread id
 */
thread_ref_t thread_myself(void);

/**
 * Delete a thread within the same address space
 *
 * \param thrd iguana thread id of the thread to be deleted
 */
void thread_delete(L4_ThreadId_t thrd);

#endif //_INCLUDE__OKLINUX_SUPPORT__IGUANA__THREAD_H_
