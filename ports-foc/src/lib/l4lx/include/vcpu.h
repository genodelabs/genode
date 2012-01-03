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
#include <foc_cpu_session/connection.h>

namespace Fiasco {
#include <l4/sys/utcb.h>
}

namespace L4lx {

	extern Genode::Foc_cpu_connection *vcpu_connection();


	class Vcpu : public Genode::Thread_base
	{
		private:

			void                      (*_func)(void *data);
			void                       *_data;
			Fiasco::l4_utcb_t          *_utcb;
			Genode::addr_t              _vcpu_state;

			static void _startup()
			{
				using namespace Genode;
				using namespace Fiasco;

				/* receive thread_base object pointer, and store it in TLS */
				addr_t      thread_base = 0;
				Msgbuf<128> snd_msg, rcv_msg;
				Ipc_server  srv(&snd_msg, &rcv_msg);
				srv >> IPC_WAIT >> thread_base;

				l4_utcb_tcr()->user[UTCB_TCR_THREAD_OBJ] = thread_base;

				Vcpu* me = dynamic_cast<Vcpu*>(Thread_base::myself());
				me->_utcb = l4_utcb();
				l4_utcb_tcr()->user[0] = me->tid(); /* L4X_UTCB_TCR_ID */

				srv << IPC_REPLY;

				/* start thread function */
				Vcpu* vcpu = reinterpret_cast<Vcpu*>(thread_base);
				vcpu->entry();
			}

		protected:

			void entry() {
				_func(_data);
				Genode::sleep_forever();
			}

		public:

			Vcpu(const char                 *name,
			     void                      (*func)(void *data),
			     void                       *data,
			     Genode::size_t              stack_size,
			     Genode::addr_t              vcpu_state)
			: Genode::Thread_base(name, stack_size),
			  _func(func),
			  _data(data),
			  _utcb(0),
			  _vcpu_state(vcpu_state) { start(); }

			void start()
			{
				using namespace Genode;

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

				/* register initial IP and SP at core */
				addr_t stack = (addr_t)&_context->stack[-4];
				stack &= ~0xf;  /* align initial stack to 16 byte boundary */
				vcpu_connection()->start(_thread_cap, (addr_t)_startup, stack);

				if (_vcpu_state)
					vcpu_connection()->enable_vcpu(_thread_cap, _vcpu_state);

				/* get gate-capability and badge of new thread */
				Thread_state state;
				vcpu_connection()->state(_thread_cap, &state);
				_tid = state.cap.dst();

				Msgbuf<128> snd_msg, rcv_msg;
				Ipc_client cli(state.cap, &snd_msg, &rcv_msg);
				cli << (addr_t)this << IPC_CALL;
			}

			Fiasco::l4_utcb_t *utcb() { return _utcb; };
	};

}

#endif /* _L4LX__VCPU_H_ */
