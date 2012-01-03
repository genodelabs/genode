/*
 * \brief  Thread facility
 * \author Christian Helmuth
 * \date   2008-10-20
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__THREAD_H_
#define _INCLUDE__DDE_KIT__THREAD_H_

#include <dde_kit/lock.h>

struct dde_kit_thread;

/**
 * Create thread
 *
 * \param   fun   thread function
 * \param   arg   argument to thread function, set to NULL if not needed
 * \param   name  thread name
 *
 * \return  thread handle
 *
 * Create a new thread running the specified function with argument arg. The
 * thread is assigned the given name.
 *
 * All DDE kit threads support thread-local storage where one data pointer may
 * be stored and retrieved.
 */
struct dde_kit_thread * dde_kit_thread_create(void (*fun)(void *), void *arg, const char *name);

/**
 * Adopt calling as DDE kit thread
 *
 * \param   name  thread name
 *
 * \return  thread handle
 */
struct dde_kit_thread * dde_kit_thread_adopt_myself(const char *name);

/**
 * Get handle of current thread
 *
 * \return  thread handle
 */
struct dde_kit_thread * dde_kit_thread_myself(void);

/**
 * Get thread-local data of a specific thread
 *
 * \param   thread  thread handle
 *
 * \return  thread-local data of this thread
 */
void * dde_kit_thread_get_data(struct dde_kit_thread * thread);

/**
 * Get thread-local data of current thread
 *
 * \return  thread-local data of current thread
 */
void * dde_kit_thread_get_my_data(void);

/**
 * Set thread-local data of specific thread
 *
 * \param   thread  thread handle
 * \param   data    thread-local data pointer
 */
void dde_kit_thread_set_data(struct dde_kit_thread *thread, void *data);

/**
 * Set thread-local data of current thread
 *
 * \param   data  thread-local data pointer
 */
void dde_kit_thread_set_my_data(void *data);

/**
 * Sleep (milliseconds)
 *
 * \param   msecs time to sleep in milliseconds
 */
void dde_kit_thread_msleep(unsigned long msecs);

/**
 * Sleep (microseconds)
 *
 * \param   usecs time to sleep in microseconds
 */
void dde_kit_thread_usleep(unsigned long usecs);

/**
 * Sleep (nanoseconds)
 *
 * \param   nsecs time to sleep in nanoseconds
 */
void dde_kit_thread_nsleep(unsigned long nsecs);

/**
 * Exit current thread
 */
void dde_kit_thread_exit(void);

/**
 * Get thread name
 *
 * \param   thread  thread handle
 */
const char *dde_kit_thread_get_name(struct dde_kit_thread *thread);

/**
 * Get unique ID
 *
 * \param   thread  thread handle
 * \return  artificial thread ID
 *
 * DDE kit does not allow direct access to the thread data structure, since
 * this struct contains platform-specific data types. However, applications
 * might want to get some kind of ID related to a dde_kit_thread, for instance
 * to use it as a Linux-like PID.
 *
 * XXX This function may be removed.
 */
int dde_kit_thread_get_id(struct dde_kit_thread *thread);

/**
 * Hint that this thread is done and may be scheduled somehow
 */
void dde_kit_thread_schedule(void);

#endif /* _INCLUDE__DDE_KIT__THREAD_H_ */
