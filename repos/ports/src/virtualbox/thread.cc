/*
 * \brief  Virtualbox adjusted pthread_create implementation
 * \author Alexander Boettcher
 * \date   2014-04-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode */
#include <base/log.h>
#include <base/thread.h>
#include <cpu_session/connection.h>

/* Genode libc pthread binding */
#include "thread.h"

#include "sup.h"
#include "vmm.h"

/* libc */
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* vbox */
#include <internal/thread.h>

static Genode::Cpu_connection * cpu_connection(RTTHREADTYPE type) {
	using namespace Genode;

	static Cpu_connection * con[RTTHREADTYPE_END - 1];
	static Lock lock;

	Assert(type && type < RTTHREADTYPE_END);

	Lock::Guard guard(lock);

	if (con[type - 1])
		return con[type - 1];

	unsigned const VIRTUAL_GENODE_VBOX_LEVELS = 16;
	static_assert (RTTHREADTYPE_END < VIRTUAL_GENODE_VBOX_LEVELS,
	               "prio levels exceeds VIRTUAL_GENODE_VBOX_LEVELS");

	long const prio = (VIRTUAL_GENODE_VBOX_LEVELS - type) *
	                  Cpu_session::PRIORITY_LIMIT / VIRTUAL_GENODE_VBOX_LEVELS;

	char * data = new (vmm_heap()) char[16];

	Genode::snprintf(data, 16, "vbox %u", type);

	con[type - 1] = new (vmm_heap()) Cpu_connection(genode_env(), data, prio);

	return con[type - 1];
}


static int create_thread(pthread_t *thread, const pthread_attr_t *attr,
	                     void *(*start_routine) (void *), void *arg)
{
	PRTTHREADINT rtthread = reinterpret_cast<PRTTHREADINT>(arg);

	Assert(rtthread);

	size_t const utcb_size = 4096;

	size_t stack_size = Genode::Thread::stack_virtual_size() -
	                    utcb_size - 2 * (1UL << 12);

	if (rtthread->cbStack < stack_size)
		stack_size = rtthread->cbStack;

	/* sanity check - emt and vcpu thread have to have same prio class */
	if (strstr(rtthread->szName, "EMT") == rtthread->szName)
		Assert(rtthread->enmType == RTTHREADTYPE_EMULATION);

	if (rtthread->enmType == RTTHREADTYPE_EMULATION) {

		unsigned int cpu_id = 0;
		sscanf(rtthread->szName, "EMT-%u", &cpu_id);

		Genode::Cpu_session * cpu_session = cpu_connection(RTTHREADTYPE_EMULATION);
		Genode::Affinity::Space space = cpu_session->affinity_space();
		Genode::Affinity::Location location(space.location_of_index(cpu_id));

		if (create_emt_vcpu(thread, stack_size, start_routine, arg,
		                    cpu_session, location, cpu_id, rtthread->szName))
			return 0;
		/*
		 * The virtualization layer had no need to setup the EMT
		 * specially, so create it as a ordinary pthread.
		 */
	}

	pthread_t thread_obj = new (vmm_heap())
		pthread(start_routine, arg, stack_size, rtthread->szName,
		        cpu_connection(rtthread->enmType),
		        Genode::Affinity::Location());

	if (!thread_obj)
		return EAGAIN;

	*thread = thread_obj;

	thread_obj->start();

	return 0;
}

extern "C" int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg)
{
	/* cleanup threads which tried to self-destruct */
	pthread_cleanup();

	PRTTHREADINT rtthread = reinterpret_cast<PRTTHREADINT>(arg);

	/* retry thread creation once after CPU session upgrade */
	for (unsigned i = 0; i < 2; i++) {
		using namespace Genode;

		try { return create_thread(thread, attr, start_routine, arg); }
		catch (Out_of_ram) {
			log("Upgrading memory for creation of "
			    "thread '", Cstring(rtthread->szName), "'");
			cpu_connection(rtthread->enmType)->upgrade_ram(4096);
		}
		catch (...) { break; }
	}

	Genode::error("could not create vbox pthread - halt");
	Genode::Lock lock(Genode::Lock::LOCKED);
	lock.lock();
	return EAGAIN;
}

extern "C" int pthread_attr_setdetachstate(pthread_attr_t *, int)
{
	return 0;
}

extern "C" int pthread_attr_setstacksize(pthread_attr_t *, size_t)
{
	return 0;
}

extern "C" int pthread_atfork(void (*)(void), void (*)(void), void (*)(void))
{
	return 0;
}
