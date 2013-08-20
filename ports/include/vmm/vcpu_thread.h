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

namespace Vmm {

	using namespace Genode;

	class Vcpu_thread;
}


class Vmm::Vcpu_thread : Genode::Thread_base
{
	private:

		/**
		 * Log2 size of portal window used for virtualization events
		 */
		enum { VCPU_EXC_BASE_LOG2 = 8 };

	public:

		Vcpu_thread(size_t stack_size)
		:
			Thread_base("vCPU", stack_size)
		{
			/* release pre-allocated selectors of Thread */
			Genode::cap_map()->remove(tid().exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2);

			/* allocate correct number of selectors */
			this->tid().exc_pt_sel = cap_map()->insert(VCPU_EXC_BASE_LOG2);

			/* tell generic thread code that this becomes a vCPU */
			this->tid().is_vcpu = true;
		}

		~Vcpu_thread()
		{
			using namespace Nova;

			revoke(Nova::Obj_crd(this->tid().exc_pt_sel, VCPU_EXC_BASE_LOG2));
			cap_map()->remove(this->tid().exc_pt_sel, VCPU_EXC_BASE_LOG2, false);

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
