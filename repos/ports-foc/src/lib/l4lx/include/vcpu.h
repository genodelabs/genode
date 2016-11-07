/*
 * \brief  Vcpus for l4lx support library.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
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
#include <foc_native_cpu/client.h>
#include <cpu_session/client.h>
#include <cpu_thread/client.h>
#include <timer_session/connection.h>
#include <foc/native_thread.h>

namespace Fiasco {
#include <l4/sys/utcb.h>
}

namespace L4lx {

	extern Genode::Cpu_session *cpu_connection();


	class Vcpu : public Genode::Thread
	{
		private:

			enum { WEIGHT = Genode::Cpu_session::Weight::DEFAULT_WEIGHT };

			Genode::Lock                _lock;
			L4_CV void                (*_func)(void *data);
			unsigned long               _data;
			Genode::addr_t              _vcpu_state;
			Timer::Connection           _timer;
			unsigned                    _cpu_nr;
			Fiasco::l4_utcb_t   * const _utcb;

		public:

			Vcpu(const char                 *str,
			     L4_CV void                (*func)(void *data),
			     unsigned long              *data,
			     Genode::size_t              stack_size,
			     Genode::addr_t              vcpu_state,
			     unsigned                    cpu_nr)
			: Genode::Thread(WEIGHT, str, stack_size,
			                 Genode::Affinity::Location(cpu_nr, 0)),
			  _lock(Genode::Cancelable_lock::LOCKED),
			  _func(func),
			  _data(data ? *data : 0),
			  _vcpu_state(vcpu_state),
			  _cpu_nr(cpu_nr),
			  _utcb((Fiasco::l4_utcb_t *)Genode::Cpu_thread_client(cap()).state().utcb)
			{
				start();

				/* set l4linux specific utcb entry: L4X_UTCB_TCR_ID */
				l4_utcb_tcr_u(_utcb)->user[0] = native_thread().kcap;

				/* enable vcpu functionality respectively */
				if (_vcpu_state) {
					Genode::Foc_native_cpu_client native_cpu(cpu_connection()->native_cpu());
					native_cpu.enable_vcpu(_thread_cap, _vcpu_state);
				}
			}

			void entry()
			{
				_lock.lock();
				_func(&_data);
				Genode::sleep_forever();
			}

			void unblock() { _lock.unlock(); }

			Genode::addr_t sp() { return (Genode::addr_t)stack_top(); }

			Genode::addr_t ip() { return (Genode::addr_t)_func; }

			Fiasco::l4_utcb_t *utcb() { return _utcb; };

			Timer::Connection* timer() { return &_timer; }

			void set_affinity(unsigned i)
			{
				Genode::Cpu_thread_client(_thread_cap).affinity(Genode::Affinity::Location(i, 0));
			}
	};

}

#endif /* _L4LX__VCPU_H_ */
