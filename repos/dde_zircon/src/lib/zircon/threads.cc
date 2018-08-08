/*
 * \brief  Zircon threading implementation
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>

#include <zx/static_resource.h>
#include <zx/thread.h>

ZX::Thread::Thread(Genode::Env &env, thrd_start_t worker, const char *label, void *arg) :
	Genode::Thread(env, label, 4096),
	_thread_worker(worker),
	_arg(reinterpret_cast<unsigned long>(arg)),
	_result(-1)
{
	if (sizeof(arg) > sizeof(_arg)){
		Genode::warning("unsigned long < void *! things can go horribly wrong!");
	}
}

void ZX::Thread::entry()
{
	_result = _thread_worker(reinterpret_cast<void *>(_arg));
}

int ZX::Thread::result()
{
	return _result;
}

extern "C" {

	int thrd_create_with_name(thrd_t* thread, thrd_start_t run, void* arg, const char* name)
	{
		Genode::Allocator &alloc = ZX::Resource<Genode::Heap>::get_component();
		Genode::Env &env = ZX::Resource<Genode::Env>::get_component();

		ZX::Thread *gthread = new (alloc) ZX::Thread(env, run, name, arg);
		*thread = reinterpret_cast<thrd_t>(gthread);

		if (sizeof(gthread) != sizeof(*thread)){
			Genode::warning("thrd_t * != ZX::Thread *! things can go horribly wrong!");
		}

		gthread->start();

		return thrd_success;
	}

	int thrd_join(thrd_t thread, int *result)
	{
		ZX::Thread *gthread = reinterpret_cast<ZX::Thread *>(thread);

		if (sizeof(gthread) != sizeof(thread)){
			Genode::warning("thrd_t * != ZX::Thread *! things can go horribly wrong!");
		}

		gthread->join();
		if (result){
			*result = gthread->result();
		}
		return thrd_success;
	}

}
