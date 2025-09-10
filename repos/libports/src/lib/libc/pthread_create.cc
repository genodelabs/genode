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
#include <stdio.h> /* __isthreaded */

/* libc-internal includes */
#include <internal/init.h>
#include <internal/thread_create.h>
#include <internal/pthread.h>


using namespace Genode;


struct Placement_policy
{
	struct Placement : Interface
	{
		unsigned pthread_id;
		unsigned cpu;

		Placement (unsigned id, unsigned cpu)
		: pthread_id(id), cpu(cpu) { }
	};

	Registry<Registered<Placement> > _policies { };

	enum class Policy { ALL, SINGLE, MANUAL };

	Policy _policy { Policy::ALL };

	void policy(String<32> const &policy_name)
	{
		if (policy_name == "single-cpu")
			_policy = Policy::SINGLE;
		if (policy_name == "manual")
			_policy = Policy::MANUAL;
		if (policy_name == "all-cpus")
			_policy = Policy::ALL;
	}

	unsigned placement(unsigned const pthread_id) const
	{
		switch (_policy) {
		case Policy::SINGLE:
			return 0U;
		case Policy::MANUAL: {
			unsigned cpu   = 0U;
			bool     found = false;
			_policies.for_each([&](auto const &policy) {
				if (policy.pthread_id == pthread_id) {
					cpu = policy.cpu;
					found = true;
				}
			});
			/* if no entry is found, Policy::ALL is applied */
			return found ? cpu : pthread_id;
		}
		case Policy::ALL:
		default:
			return pthread_id;
		}
	}

	void add_placement(Genode::Allocator &alloc, unsigned pthread, unsigned cpu)
	{
		new (alloc) Registered<Placement> (_policies, pthread, cpu);
	}
};


static Genode::Env *_geneode_env { nullptr };
static bool         _verbose     { false };


Placement_policy &placement_policy()
{
	static Placement_policy policy { };
	return policy;
}


void Libc::init_pthread_support(Genode::Env &env, Node const &node,
                                Genode::Allocator &alloc)
{
	_geneode_env = &env;

	_verbose = node.attribute_value("verbose", false);

	String<32> const policy_name = node.attribute_value("placement",
	                                                    String<32>("all-cpus"));
	placement_policy().policy(policy_name);

	node.for_each_sub_node("thread", [&] (Node const &policy) {

		if (policy.has_attribute("id") && policy.has_attribute("cpu")) {
			unsigned const id  = policy.attribute_value("id", 0U);
			unsigned const cpu = policy.attribute_value("cpu", 0U);

			if (_verbose)
				log("pthread.", id, " -> cpu ", cpu);

			placement_policy().add_placement(alloc, id, cpu);
		}
	});
}


static unsigned pthread_id()
{
	static Mutex mutex;

	static unsigned id = 0;

	Mutex::Guard guard(mutex);

	return id++;
}


static int pthread_create_from_env(Genode::Env &env, pthread_t *thread,
                                   void *(*start_routine) (void *),
                                   void *arg,
                                   size_t stack_size,
                                   char const *name,
                                   Affinity::Location location)
{
	Libc::Allocator alloc { };
	pthread_t thread_obj = new (alloc)
		pthread(env, start_routine, arg, stack_size, name, location);
	if (!thread_obj)
		return EAGAIN;

	*thread = thread_obj;

	thread_obj->start();

	return 0;
}


int Libc::pthread_create_from_thread(pthread_t *thread, Thread &t, void *stack_address)
{
	Libc::Allocator alloc { };

	pthread_t thread_obj = new (alloc) pthread(t, stack_address);

	if (!thread_obj)
		return EAGAIN;

	*thread = thread_obj;

	/* use threaded mode in FreeBSD libc code */
	__isthreaded = 1;

	return 0;
}


int Libc::pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                         void *(*start_routine) (void *), void *arg,
                         char const *name)
{
	if (!_geneode_env || !start_routine || !thread)
		return EINVAL;

	size_t const stack_size = (attr && *attr && (*attr)->stack_size)
	                        ? (*attr)->stack_size
	                        : Libc::Component::stack_size();

	unsigned const id { pthread_id() };
	unsigned const cpu = placement_policy().placement(id);

	String<32> const pthread_name { "pthread.", id };
	Affinity::Space space { _geneode_env->cpu().affinity_space() };
	Affinity::Location location { space.location_of_index(cpu) };

	if (_verbose)
		log("create ", pthread_name, " -> cpu ", cpu);

	int result =
		pthread_create_from_env(*_geneode_env, thread, start_routine,
		                        arg, stack_size,
		                        name ? : pthread_name.string(),
		                        location);

	if ((result == 0) && attr && *attr &&
	    ((*attr)->detach_state == PTHREAD_CREATE_DETACHED))
		pthread_detach(*thread);

	return result;
}


extern "C" int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg)
{
	return Libc::pthread_create(thread, attr, start_routine, arg, nullptr);
}


extern "C" typeof(pthread_create) _pthread_create __attribute__((alias("pthread_create")));
