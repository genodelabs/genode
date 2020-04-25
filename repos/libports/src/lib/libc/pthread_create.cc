/*
 * \brief  pthread_create implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2012-03-12
 *
 * Purpose of a single file for pthread_create is that other application may
 * easily replace this implementation with another one.
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/mutex.h>
#include <util/string.h>

/* libc includes */
#include <libc/allocator.h>
#include <errno.h>

/* libc-internal includes */
#include <internal/init.h>
#include <internal/thread_create.h>
#include <internal/pthread.h>


static Genode::Cpu_session * _cpu_session { nullptr };
static unsigned              _id_cpu_map[32];
static bool                  _verbose { false };

void Libc::init_pthread_support(Genode::Cpu_session &cpu_session,
                                Genode::Xml_node node)
{
	_cpu_session = &cpu_session;

	_verbose = node.attribute_value("verbose", false);

	String<32> placement("all-cpus");
	placement = node.attribute_value("placement", placement);

	if (placement == "single-cpu") {
		return;
	}

	if (placement == "manual") {
		node.for_each_sub_node("thread", [](Xml_node &node) {
			if (node.has_attribute("id") && node.has_attribute("cpu")) {
				unsigned const id  = node.attribute_value("id", 0U);
				unsigned const cpu = node.attribute_value("cpu", 0U);

				if (id < sizeof(_id_cpu_map) / sizeof(_id_cpu_map[0])) {
					_id_cpu_map[id] = cpu;
					if (_verbose)
						Genode::log("pthread.", id, " -> cpu ", cpu);
				} else {
					Genode::warning("pthread configuration ignored - "
					                "id=", id, " cpu=", cpu);
				}
			}
		});
		return;
	}

	/* all-cpus and unknown case */
	for (unsigned i = 0; i < sizeof(_id_cpu_map) / sizeof(_id_cpu_map[0]); i++)
		_id_cpu_map[i] = i;
}


static unsigned pthread_id()
{
	static Genode::Mutex mutex;

	static unsigned id = 0;

	Genode::Mutex::Guard guard(mutex);

	return id++;
}


int Libc::pthread_create(pthread_t *thread,
                         void *(*start_routine) (void *), void *arg,
                         size_t stack_size, char const * name,
                         Cpu_session * cpu, Affinity::Location location)
{
	Libc::Allocator alloc { };
	pthread_t thread_obj = new (alloc)
	                       pthread(start_routine, arg,
	                               stack_size, name, cpu, location);
	if (!thread_obj)
		return EAGAIN;

	*thread = thread_obj;

	thread_obj->start();

	return 0;
}


int Libc::pthread_create(pthread_t *thread, Thread &t)
{
	Libc::Allocator alloc { };
	pthread_t thread_obj = new (alloc) pthread(t);

	if (!thread_obj)
		return EAGAIN;

	*thread = thread_obj;
	return 0;
}


extern "C"
{
	int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	                   void *(*start_routine) (void *), void *arg)
	{
		if (!_cpu_session || !start_routine || !thread)
			return EINVAL;

		size_t const stack_size = (attr && *attr && (*attr)->stack_size)
		                        ? (*attr)->stack_size
		                        : Libc::Component::stack_size();

		using Genode::Affinity;

		unsigned const id { pthread_id() };
		unsigned const cpu = (id < sizeof(_id_cpu_map) / sizeof(_id_cpu_map[0])) ? _id_cpu_map[id] : 0;

		Genode::String<32> const pthread_name { "pthread.", id };
		Affinity::Space space { _cpu_session->affinity_space() };
		Affinity::Location location { space.location_of_index(cpu) };

		if (_verbose)
			Genode::log("create ", pthread_name, " -> cpu ", cpu);

		return Libc::pthread_create(thread, start_routine, arg, stack_size,
		                            pthread_name.string(), _cpu_session,
		                            location);
	}

	typeof(pthread_create) _pthread_create
		__attribute__((alias("pthread_create")));
}
