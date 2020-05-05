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
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/thread.h>
#include <cpu_session/connection.h>

/* Genode libc pthread binding */
#include <internal/thread_create.h>

#include "sup.h"
#include "vmm.h"

/* libc */
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* vbox */
#include <internal/thread.h>

static bool use_priorities()
{
	Genode::Attached_rom_dataspace const platform(genode_env(), "platform_info");
	Genode::Xml_node const kernel = platform.xml().sub_node("kernel");
	return kernel.attribute_value("name", Genode::String<16>("unknown")) == "nova";
}

static long prio_class(RTTHREADTYPE const type)
{
	unsigned const VIRTUAL_GENODE_VBOX_LEVELS = 16;
	static_assert (RTTHREADTYPE_END < VIRTUAL_GENODE_VBOX_LEVELS,
	               "prio levels exceeds VIRTUAL_GENODE_VBOX_LEVELS");

	/* evaluate once */
	static bool const priorities = use_priorities();

	if (!priorities)
		return Genode::Cpu_session::DEFAULT_PRIORITY;

	return (VIRTUAL_GENODE_VBOX_LEVELS - type) *
	        Genode::Cpu_session::PRIORITY_LIMIT / VIRTUAL_GENODE_VBOX_LEVELS;
}

static Genode::Cpu_connection * cpu_connection(RTTHREADTYPE type)
{
	using namespace Genode;

	static Cpu_connection * con[RTTHREADTYPE_END - 1];
	static Lock lock;

	Assert(type && type < RTTHREADTYPE_END);

	Lock::Guard guard(lock);

	if (con[type - 1])
		return con[type - 1];

	char * data = new (vmm_heap()) char[16];

	Genode::snprintf(data, 16, "vbox %u", type);

	con[type - 1] = new (vmm_heap()) Cpu_connection(genode_env(), data,
	                                                prio_class(type));

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

		Genode::Cpu_connection * cpu = cpu_connection(RTTHREADTYPE_EMULATION);
		Genode::Affinity::Space space = cpu->affinity_space();
		Genode::Affinity::Location location(space.location_of_index(cpu_id));

		if (create_emt_vcpu(thread, stack_size, start_routine, arg,
		                    cpu, location, cpu_id, rtthread->szName,
		                    prio_class(rtthread->enmType)))
			return 0;
		/*
		 * The virtualization layer had no need to setup the EMT
		 * specially, so create it as a ordinary pthread.
		 */
	}

	/*
	 * Make sure timers run at the same priority as component threads, otherwise
	 * no timer progress can be made. See 'rtTimeNanoTSInternalRef' (timesupref.h)
	 * and 'rtTimerLRThread' (timerlr-generic.cpp)
	 */
	bool const rtthread_timer = rtthread->enmType == RTTHREADTYPE_TIMER;

	if (rtthread_timer) {
		return Libc::pthread_create(thread, start_routine, arg,
		                            stack_size, rtthread->szName, nullptr,
		                            Genode::Affinity::Location());

	} else {
		using namespace Genode;
		Cpu_connection *cpu = cpu_connection(rtthread->enmType);
		return cpu->retry_with_upgrade(Ram_quota{8*1024}, Cap_quota{2},
		                               [&] ()
		{
			return Libc::pthread_create(thread, start_routine, arg,
			                            stack_size, rtthread->szName, cpu,
			                            Genode::Affinity::Location());
		});
	}

}

extern "C" int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg)
{
	PRTTHREADINT rtthread = reinterpret_cast<PRTTHREADINT>(arg);

	/* retry thread creation once after CPU session upgrade */
	for (unsigned i = 0; i < 2; i++) {
		using namespace Genode;

		try {
			return create_thread(thread, attr, start_routine, arg); }
		catch (Out_of_ram) {
			log("Upgrading memory for creation of "
			    "thread '", Cstring(rtthread->szName), "'");
			cpu_connection(rtthread->enmType)->upgrade_ram(4096);
		} catch (Genode::Signal_receiver::Signal_not_pending) {
			error("signal not pending ?");
		} catch (Out_of_caps) {
			error("out of caps ...");
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
