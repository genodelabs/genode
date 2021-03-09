/*
 * \brief  VirtualBox libc runtime: pthread adaptions
 * \author Christian Helmuth
 * \date   2021-03-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* libc includes */
#include <errno.h>
#include <sched.h>
#include <pthread.h>

/* libc internal */
#include <internal/thread_create.h> /* Libc::pthread_create() */
#include <internal/call_func.h>     /* call_func() */

/* VirtualBox includes */
#include <VBox/vmm/uvm.h>
#include <internal/thread.h> /* RTTHREADINT etc. */

/* Genode includes */
#include <base/env.h>
#include <base/entrypoint.h>
#include <base/registry.h>
#include <util/string.h>
#include <util/reconstructible.h>

/* local includes */
#include <init.h>
#include <pthread_emt.h>
#include <sup.h>
#include <stub_macros.h>

static bool const debug = true; /* required by stub_macros.h */


using namespace Genode;


extern "C" int sched_yield()
{
	static unsigned long counter = 0;

	if (++counter % 100'000 == 0)
		warning(__func__, " called ", counter, " times");

	return 0;
}

extern "C" int sched_get_priority_max(int policy) TRACE(0)
extern "C" int sched_get_priority_min(int policy) TRACE(0)
extern "C" int pthread_setschedparam(pthread_t thread, int policy,
                          const struct sched_param *param) TRACE(0)
extern "C" int pthread_getschedparam(pthread_t thread, int *policy,
                          struct sched_param *param) TRACE(0)


static void print(Output &o, RTTHREADTYPE type)
{
	switch (type) {
	case RTTHREADTYPE_INFREQUENT_POLLER: print(o, "POLLER");            return;
	case RTTHREADTYPE_MAIN_HEAVY_WORKER: print(o, "MAIN_HEAVY_WORKER"); return;
	case RTTHREADTYPE_EMULATION:         print(o, "EMULATION");         return;
	case RTTHREADTYPE_DEFAULT:           print(o, "DEFAULT");           return;
	case RTTHREADTYPE_GUI:               print(o, "GUI");               return;
	case RTTHREADTYPE_MAIN_WORKER:       print(o, "MAIN_WORKER");       return;
	case RTTHREADTYPE_VRDP_IO:           print(o, "VRDP_IO");           return;
	case RTTHREADTYPE_DEBUGGER:          print(o, "DEBUGGER");          return;
	case RTTHREADTYPE_MSG_PUMP:          print(o, "MSG_PUMP");          return;
	case RTTHREADTYPE_IO:                print(o, "IO");                return;
	case RTTHREADTYPE_TIMER:             print(o, "TIMER");             return;

	case RTTHREADTYPE_INVALID: print(o, "invalid?"); return;
	case RTTHREADTYPE_END:     print(o, "end?");     return;
	}
}


namespace Pthread {

	struct Entrypoint;
	struct Factory;

} /* namespace Pthread */


class Pthread::Entrypoint : public Pthread::Emt
{
	private:

		/* members initialized by constructing thread */

		Sup::Cpu_index const _cpu;
		size_t         const _stack_size; /* stack size for EMT mode */

		Genode::Entrypoint _ep;
		Blockade           _construction_finalized { };

		void *(*_emt_start_routine) (void *);
		void   *_emt_arg;

		enum class Mode { VCPU, EMT } _mode { Mode::VCPU };

		jmp_buf _vcpu_jmp_buf;
		jmp_buf _emt_jmp_buf;

		/* members finally initialized by the entrypoint itself */

		void      *_emt_stack   { nullptr };
		pthread_t  _emt_pthread { };

		void _finalize_construction()
		{
			Genode::Thread &myself = *Genode::Thread::myself();

			_emt_stack = myself.alloc_secondary_stack(myself.name().string(),
			                                          _stack_size);

			Libc::pthread_create_from_thread(&_emt_pthread, myself, _emt_stack);

			_construction_finalized.wakeup();

			/* switch to EMT mode and call pthread start_routine */
			if (setjmp(_vcpu_jmp_buf) == 0) {
				_mode = Mode::EMT;
				call_func(_emt_stack, (void *)_emt_start_routine, _emt_arg);
			}
		}

		Genode::Signal_handler<Entrypoint> _finalize_construction_sigh {
			_ep, *this, &Entrypoint::_finalize_construction };

	public:

		Entrypoint(Env &env, Sup::Cpu_index cpu, size_t stack_size,
		           char const *name, Affinity::Location location,
		           void *(*start_routine) (void *), void *arg)
		:
			_cpu(cpu), _stack_size(stack_size),
			_ep(env, 64*1024, name, location),
			_emt_start_routine(start_routine), _emt_arg(arg)
		{
			Signal_transmitter(_finalize_construction_sigh).submit();

			_construction_finalized.block();
		}

		/* registered object must have virtual destructor */
		virtual ~Entrypoint() { }

		Sup::Cpu_index cpu() const { return _cpu; }

		pthread_t pthread() const { return _emt_pthread; }

		/* Pthread::Emt interface */

		void switch_to_emt() override
		{
			Assert(_mode == Mode::VCPU);

			if (setjmp(_vcpu_jmp_buf) == 0) {
				_mode = Mode::EMT;
				longjmp(_emt_jmp_buf, 1);
			}
		}

		void switch_to_vcpu() override
		{
			Assert(pthread_self() == _emt_pthread);
			Assert(_mode == Mode::EMT);

			if (setjmp(_emt_jmp_buf) == 0) {
				_mode = Mode::VCPU;
				longjmp(_vcpu_jmp_buf, 1);
			}
		}

		Genode::Entrypoint & genode_ep() override { return _ep; }
};


class Pthread::Factory
{
	private:

		Env &_env;

		Registry<Registered<Pthread::Entrypoint>> _entrypoints;

		Affinity::Space const _affinity_space { _env.cpu().affinity_space() };

	public:

		Factory(Env &env) : _env(env) { }

		Entrypoint & create(Sup::Cpu_index cpu, size_t stack_size, char const *name,
		                    void *(*start_routine) (void *), void *arg)
		{

			Affinity::Location const location =
				_affinity_space.location_of_index(cpu.value);

			return *new Registered<Entrypoint>(_entrypoints, _env, cpu,
			                                   stack_size, name,
			                                   location, start_routine, arg);
		}

		struct Emt_for_cpu_not_found : Exception { };

		Emt & emt_for_cpu(Sup::Cpu_index cpu)
		{
			Entrypoint *found = nullptr;

			_entrypoints.for_each([&] (Entrypoint &ep) {
				if (ep.cpu().value == cpu.value)
					found = &ep;
			});

			if (!found)
				throw Emt_for_cpu_not_found();

			return *found;
		}
};


static Pthread::Factory *factory;


Pthread::Emt & Pthread::emt_for_cpu(Sup::Cpu_index cpu)
{
	return factory->emt_for_cpu(cpu);
}


void Pthread::init(Env &env)
{
	factory = new Pthread::Factory(env);
}


static int create_emt_thread(pthread_t *thread, const pthread_attr_t *attr,
                             void *(*start_routine) (void *),
                             PRTTHREADINT rtthread)
{
	PUVMCPU pUVCpu = (PUVMCPU)rtthread->pvUser;

	log("************ ", __func__, ":"
	   , " idCpu=", pUVCpu->idCpu
	   );

	Sup::Cpu_index const cpu { pUVCpu->idCpu };

	size_t stack_size = 0;

	/* try to fetch configured stack size form attribute */
	pthread_attr_getstacksize(attr, &stack_size);

	Assert(stack_size);

	Pthread::Entrypoint &ep =
		factory->create(cpu, stack_size, rtthread->szName,
		                start_routine, rtthread);

	*thread = ep.pthread();

	return 0;
}



extern "C" int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg)
{
	PRTTHREADINT rtthread = reinterpret_cast<PRTTHREADINT>(arg);

error("************ ", __func__, ":"
     , " szName='", Cstring(rtthread->szName), "'"
     , " enmType=", rtthread->enmType
     , " cbStack=", rtthread->cbStack
     );

	/*
	 * Emulation threads (EMT) represent the guest CPU, so we implement them in
	 * dedicated entrypoints that also handle vCPU events in combination with
	 * user-level threading (i.e., setjmp/longjmp).
	 */
	if (rtthread->enmType == RTTHREADTYPE_EMULATION)
		return create_emt_thread(thread, attr, start_routine, rtthread);
	else
		return Libc::pthread_create(thread, attr, start_routine, arg, rtthread->szName);
}
