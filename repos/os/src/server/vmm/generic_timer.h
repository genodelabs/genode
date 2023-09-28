/*
 * \brief  VMM ARM Generic timer device model
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2019-08-20
 */

/*
 * Copyright (C) 2019-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__GENERIC_TIMER_H_
#define _SRC__SERVER__VMM__GENERIC_TIMER_H_

#include <gic.h>

#include <cpu/vcpu_state_virtualization.h>
#include <drivers/timer/util.h>
#include <timer_session/connection.h>
#include <util/register.h>

namespace Vmm {
	class Cpu_base;
	class Generic_timer;
}

using Genode::Vcpu_state;

class Vmm::Generic_timer : Gic::Irq::Irq_handler
{
	private:

		Timer::Connection                      _timer;
		Timer::One_shot_timeout<Generic_timer> _timeout;
		Gic::Irq                             & _irq;
		Cpu_base                             & _cpu;

		struct Ctrl : Genode::Register<32>
		{
			struct Enabled : Bitfield<0,1> {};
			struct Imask   : Bitfield<1,1> {};
			struct Istatus : Bitfield<2,1> {};
		};

		Genode::uint64_t _ticks_per_ms();

		bool _enabled(Vcpu_state &state);
		bool _masked(Vcpu_state &state);
		bool _pending(Vcpu_state &state);

		void _handle_timeout(Genode::Duration);
		Genode::uint64_t _usecs_left(Vcpu_state &state);

	public:

		Generic_timer(Genode::Env        & env,
		              Genode::Entrypoint & ep,
		              Gic::Irq           & irq,
		              Cpu_base           & cpu);

		void schedule_timeout(Vcpu_state &state);
		void cancel_timeout();
		void handle_irq(Vcpu_state &state);
		void dump(Vcpu_state &state);
		static void setup_state(Vcpu_state &state);


		/*****************
		 ** Irq_handler **
		 *****************/

		void eoi() override;
};

#endif /* _SRC__SERVER__VMM__GENERIC_TIMER_H_ */
