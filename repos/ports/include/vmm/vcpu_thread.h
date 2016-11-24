/*
 * \brief  Utilities for implementing VMMs on Genode/NOVA
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM__VCPU_THREAD_H_
#define _INCLUDE__VMM__VCPU_THREAD_H_

/* Genode includes */
#include <base/thread.h>
#include <cap_session/connection.h>
#include <cpu_session/connection.h>
#include <pd_session/connection.h>
#include <region_map/client.h>
#include <nova_native_cpu/client.h>
#include <cpu_thread/client.h>

/* NOVA includes */
#include <nova/native_thread.h>
#include <nova/cap_map.h>

namespace Vmm {

	using namespace Genode;

	class Vcpu_thread;
	class Vcpu_other_pd;
	class Vcpu_same_pd;
}

class Vmm::Vcpu_thread
{
	public:

		virtual Genode::addr_t exc_base()            = 0;
		virtual void           start(Genode::addr_t) = 0;
};

class Vmm::Vcpu_other_pd : public Vmm::Vcpu_thread
{
	private:

		Genode::Capability<Genode::Pd_session> _pd_cap;
		Genode::Affinity::Location  _location;
		Genode::Cpu_session        *_cpu_session;

		Genode::addr_t _exc_pt_sel;

	public:

		Vcpu_other_pd(Cpu_session * cpu_session,
		              Genode::Affinity::Location location,
		              Genode::Capability<Genode::Pd_session> pd_cap,
		              Genode::size_t = 0 /* stack_size */)
		:
			_pd_cap(pd_cap), _location(location), _cpu_session(cpu_session),
			_exc_pt_sel(Genode::cap_map()->insert(Nova::NUM_INITIAL_VCPU_PT_LOG2))
		{ }

		void start(Genode::addr_t sel_ec)
		{
			using namespace Genode;

			Thread_capability vcpu_vm =
				_cpu_session->create_thread(_pd_cap, "vCPU",
				                            _location, Cpu_session::Weight());

			/* tell parent that this will be a vCPU */
			Thread_state state;
			state.sel_exc_base = _exc_pt_sel;
			state.vcpu         = true;

			Cpu_thread_client cpu_thread(vcpu_vm);

			cpu_thread.state(state);

			/* obtain interface to NOVA-specific CPU session operations */
			Nova_native_cpu_client native_cpu(_cpu_session->native_cpu());

			/* create new pager object and assign it to the new thread */
			Native_capability pager_cap = native_cpu.pager_cap(vcpu_vm);

			/*
			 * Translate pager cap of current executing thread, which is used
			 * to lookup current PD - required during PD creation.
			 */
			translate_remote_pager(pager_cap,
			                      Thread::myself()->native_thread().ec_sel + 1);

			/* start vCPU in separate PD */
			cpu_thread.start(0, 0);

			/*
			 * Request native EC thread cap and put it next to the
			 * SM cap - see Vcpu_dispatcher->sel_sm_ec description
			 */
			request_native_ec_cap(pager_cap, sel_ec);

			/* request creation of SC to let vCPU run */
			cpu_thread.resume();
		}

		Genode::addr_t exc_base() { return _exc_pt_sel; }
};


class Vmm::Vcpu_same_pd : public Vmm::Vcpu_thread, Genode::Thread
{
	enum { WEIGHT = Genode::Cpu_session::Weight::DEFAULT_WEIGHT };

	public:

		Vcpu_same_pd(Cpu_session * cpu_session,
		             Genode::Affinity::Location location,
		             Genode::Capability<Genode::Pd_session>,
		             Genode::size_t stack_size)
		:
			Thread(WEIGHT, "vCPU", stack_size, Type::NORMAL, cpu_session, location)
		{
			/* release pre-allocated selectors of Thread */
			Genode::cap_map()->remove(native_thread().exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2);

			/* allocate correct number of selectors */
			this->native_thread().exc_pt_sel = cap_map()->insert(Nova::NUM_INITIAL_VCPU_PT_LOG2);

			/* tell generic thread code that this becomes a vCPU */
			this->native_thread().vcpu = true;
		}

		~Vcpu_same_pd()
		{
			using namespace Nova;

			revoke(Nova::Obj_crd(this->native_thread().exc_pt_sel, NUM_INITIAL_VCPU_PT_LOG2));
			cap_map()->remove(this->native_thread().exc_pt_sel, NUM_INITIAL_VCPU_PT_LOG2, false);

			/* allocate selectors for ~Thread */
			this->native_thread().exc_pt_sel = cap_map()->insert(Nova::NUM_INITIAL_PT_LOG2);
		}

		addr_t exc_base() { return this->native_thread().exc_pt_sel; }

		void start(Genode::addr_t sel_ec)
		{
			this->Thread::start();

			/* obtain interface to NOVA-specific CPU session operations */
			Nova_native_cpu_client native_cpu(_cpu_session->native_cpu());

			/*
			 * Request native EC thread cap and put it next to the
			 * SM cap - see Vcpu_dispatcher->sel_sm_ec description
			 */
			Native_capability pager_cap = native_cpu.pager_cap(_thread_cap);
			request_native_ec_cap(pager_cap, sel_ec);
		}

		void entry() { }
};

#endif /* _INCLUDE__VMM__VCPU_THREAD_H_ */
