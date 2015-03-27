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
#include <base/cap_map.h>
#include <base/thread.h>
#include <cap_session/connection.h>
#include <nova_cpu_session/connection.h>
#include <cpu_session/connection.h>
#include <pd_session/connection.h>

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

		Genode::Pd_connection       _pd_session;
		Genode::Affinity::Location  _location;
		Genode::Cpu_session        *_cpu_session;

		Genode::addr_t _exc_pt_sel;

	public:

		Vcpu_other_pd(Cpu_session * cpu_session,
		              Genode::Affinity::Location location)
		:
			_pd_session("VM"), _location(location), _cpu_session(cpu_session),
			_exc_pt_sel(Genode::cap_map()->insert(Nova::NUM_INITIAL_VCPU_PT_LOG2))
		{ }

		void start(Genode::addr_t sel_ec)
		{
			using namespace Genode;

			enum { WEIGHT = Cpu_session::DEFAULT_WEIGHT };
			Thread_capability vcpu_vm =
				_cpu_session->create_thread(WEIGHT, "vCPU");

			/* assign thread to protection domain */
			_pd_session.bind_thread(vcpu_vm);

			/* create new pager object and assign it to the new thread */
			Pager_capability pager_cap = env()->rm_session()->add_client(vcpu_vm);

			_cpu_session->set_pager(vcpu_vm, pager_cap);

			/* tell parent that this will be a vCPU */
			Thread_state state;
			state.sel_exc_base = Native_thread::INVALID_INDEX;
			state.is_vcpu      = true;

			_cpu_session->state(vcpu_vm, state);

			/*
			 * Delegate parent the vCPU exception portals required during PD
			 * creation.
			 */
			delegate_vcpu_portals(pager_cap, exc_base());

			/* place the thread on CPU described by location object */
			_cpu_session->affinity(vcpu_vm, _location);

			/* start vCPU in separate PD */
			_cpu_session->start(vcpu_vm, 0, 0);

			/*
			 * Request native EC thread cap and put it next to the
			 * SM cap - see Vcpu_dispatcher->sel_sm_ec description
			 */
			request_native_ec_cap(pager_cap, sel_ec);
		}

		Genode::addr_t exc_base() { return _exc_pt_sel; }
};


class Vmm::Vcpu_same_pd : public Vmm::Vcpu_thread, Genode::Thread_base
{
	enum { WEIGHT = Genode::Cpu_session::DEFAULT_WEIGHT };

	public:

		Vcpu_same_pd(size_t stack_size, Cpu_session * cpu_session,
		             Genode::Affinity::Location location)
		:
			Thread_base(WEIGHT, "vCPU", stack_size, Type::NORMAL, cpu_session)
		{
			/* release pre-allocated selectors of Thread */
			Genode::cap_map()->remove(tid().exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2);

			/* allocate correct number of selectors */
			this->tid().exc_pt_sel = cap_map()->insert(Nova::NUM_INITIAL_VCPU_PT_LOG2);

			/* tell generic thread code that this becomes a vCPU */
			this->tid().is_vcpu = true;

			/* place the thread on CPU described by location object */
			cpu_session->affinity(Thread_base::cap(), location);
		}

		~Vcpu_same_pd()
		{
			using namespace Nova;

			revoke(Nova::Obj_crd(this->tid().exc_pt_sel, NUM_INITIAL_VCPU_PT_LOG2));
			cap_map()->remove(this->tid().exc_pt_sel, NUM_INITIAL_VCPU_PT_LOG2, false);

			/* allocate selectors for ~Thread */
			this->tid().exc_pt_sel = cap_map()->insert(Nova::NUM_INITIAL_PT_LOG2);
		}

		addr_t exc_base() { return this->tid().exc_pt_sel; }

		void start(Genode::addr_t sel_ec)
		{
			this->Thread_base::start();

			/*
			 * Request native EC thread cap and put it next to the
			 * SM cap - see Vcpu_dispatcher->sel_sm_ec description
			 */
			request_native_ec_cap(_pager_cap, sel_ec);
		}

		void entry() { }
};

#endif /* _INCLUDE__VMM__VCPU_THREAD_H_ */
