/*
 * \brief  Create custom pthreads with native CPU sessions and affinities
 * \author Emery Hemingway
 * \date   2018-11-22
 *
 * The Libc::pthread_create symbol is exported for the sake of Vbox
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__THREAD_CREATE_H_
#define _LIBC__INTERNAL__THREAD_CREATE_H_

/* Genode includes */
#include <base/thread.h>

/* libc includes */
#include <pthread.h>

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	int pthread_create(pthread_t *thread,
	                   void *(*start_routine) (void *), void *arg,
	                   size_t stack_size, char const * name,
	                   Cpu_session * cpu, Affinity::Location location);

	int pthread_create(pthread_t *, Thread &);
}

#endif /* _LIBC__INTERNAL__THREAD_CREATE_H_ */
