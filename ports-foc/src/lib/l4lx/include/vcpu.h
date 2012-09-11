/*
 * \brief  Vcpus for l4lx support library.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__VCPU_H_
#define _L4LX__VCPU_H_

/* Genode includes */
#include <base/ipc.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <base/cap_map.h>
#include <foc_cpu_session/connection.h>
#include <timer_session/connection.h>

namespace Fiasco {
#include <l4/sys/utcb.h>
}

namespace L4lx {

	extern Genode::Foc_cpu_connection *vcpu_connection();


	class Vcpu : public Genode::Thread_base
	{
		private:

			void                      (*_func)(void *data);
			unsigned long               _data;
			Genode::addr_t              _vcpu_state;
			Timer::Connection           _timer;
			unsigned                    _cpu_nr;

			static void _startup()
			{
				/* start thread function */
				Vcpu* vcpu = reinterpret_cast<Vcpu*>(Genode::Thread_base::myself());
				vcpu->entry();
			}

		protected:

			void entry() {
				_func(&_data);
				Genode::sleep_forever();
			}

		public:

			Vcpu(const char                 *str,
			     void                      (*func)(void *data),
			     unsigned long              *data,
			     Genode::size_t              stack_size,
			     Genode::addr_t              vcpu_state,
			     unsigned                    cpu_nr)
			: Genode::Thread_base(str, stack_size),
			  _func(func),
			  _data(data ? *data : 0),
			  _vcpu_state(vcpu_state),
			  _cpu_nr(cpu_nr)
			{
				using namespace Genode;
				using namespace Fiasco;

				/* create thread at core */
				char buf[48];
				name(buf, sizeof(buf));
				_thread_cap = vcpu_connection()->create_thread(buf);

				/* assign thread to protection domain */
				env()->pd_session()->bind_thread(_thread_cap);

				/* create new pager object and assign it to the new thread */
				Pager_capability pager_cap =
					env()->rm_session()->add_client(_thread_cap);
				vcpu_connection()->set_pager(_thread_cap, pager_cap);

				/* get gate-capability and badge of new thread */
				Thread_state state;
				vcpu_connection()->state(_thread_cap, &state);
				_tid = state.kcap;
				_context->utcb = state.utcb;

				Cap_index *i = cap_map()->insert(state.id, state.kcap);
				l4_utcb_tcr_u(state.utcb)->user[UTCB_TCR_BADGE] = (unsigned long) i;
				l4_utcb_tcr_u(state.utcb)->user[UTCB_TCR_THREAD_OBJ] = (addr_t)this;
				l4_utcb_tcr_u(state.utcb)->user[0] = state.kcap; /* L4X_UTCB_TCR_ID */
			}

			void start()
			{
				using namespace Genode;

				/* register initial IP and SP at core */
				addr_t stack = (addr_t)&_context->stack[-4];
				stack &= ~0xf;  /* align initial stack to 16 byte boundary */
				vcpu_connection()->start(_thread_cap, (addr_t)_startup, stack);

				if (_vcpu_state)
					vcpu_connection()->enable_vcpu(_thread_cap, _vcpu_state);

				set_affinity(_cpu_nr);
			}

			Genode::addr_t sp() {
				return ((Genode::addr_t)&_context->stack[-4]) & ~0xf; }
			Genode::addr_t ip() { return (Genode::addr_t)_startup; }

			Fiasco::l4_utcb_t *utcb() { return _context->utcb; };

			Timer::Connection* timer() { return &_timer; }

			void set_affinity(unsigned i) {
				vcpu_connection()->affinity(_thread_cap, i); }
	};

}

#endif /* _L4LX__VCPU_H_ */
