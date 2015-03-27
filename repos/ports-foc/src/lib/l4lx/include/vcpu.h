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
#include <base/cap_map.h>
#include <foc_cpu_session/connection.h>
#include <timer_session/connection.h>

namespace Fiasco {
#include <l4/sys/utcb.h>
}

namespace L4lx {

	extern Genode::Foc_cpu_session_client *vcpu_connection();


	class Vcpu : public Genode::Thread_base
	{
		private:

			enum { WEIGHT = Genode::Cpu_session::DEFAULT_WEIGHT };

			Genode::Lock                _lock;
			L4_CV void                (*_func)(void *data);
			unsigned long               _data;
			Genode::addr_t              _vcpu_state;
			Timer::Connection           _timer;
			unsigned                    _cpu_nr;

		public:

			Vcpu(const char                 *str,
			     L4_CV void                (*func)(void *data),
			     unsigned long              *data,
			     Genode::size_t              stack_size,
			     Genode::addr_t              vcpu_state,
			     unsigned                    cpu_nr)
			: Genode::Thread_base(WEIGHT, str, stack_size),
			  _lock(Genode::Cancelable_lock::LOCKED),
			  _func(func),
			  _data(data ? *data : 0),
			  _vcpu_state(vcpu_state),
			  _cpu_nr(cpu_nr)
			{
				start();

				/* set l4linux specific utcb entry: L4X_UTCB_TCR_ID */
				l4_utcb_tcr_u(utcb())->user[0] = tid();

				/* enable vcpu functionality respectively */
				if (_vcpu_state)
					vcpu_connection()->enable_vcpu(_thread_cap, _vcpu_state);

				/* set cpu affinity */
				set_affinity(_cpu_nr);
			}

			void entry()
			{
				_lock.lock();
				_func(&_data);
				Genode::sleep_forever();
			}

			void unblock() { _lock.unlock(); }

			Genode::addr_t sp() { return _context->stack_top(); }

			Genode::addr_t ip() { return (Genode::addr_t)_func; }

			Fiasco::l4_utcb_t *utcb() { return _context->utcb; };

			Timer::Connection* timer() { return &_timer; }

			void set_affinity(unsigned i)
			{
				vcpu_connection()->affinity(_thread_cap,
				                            Genode::Affinity::Location(i, 0));
			}
	};

}

#endif /* _L4LX__VCPU_H_ */
