/*
 * \brief  Instance of the timer session interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-30
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_SESSION_COMPONENT_
#define _TIMER_SESSION_COMPONENT_

/* Genode includes */
#include <base/rpc_server.h>
#include <timer_session/capability.h>
#include <os/attached_rom_dataspace.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/kip.h>
}


static Fiasco::l4_timeout_s mus_to_timeout(unsigned int mus)
{
	using namespace Fiasco;

	if (mus == 0)
		return L4_IPC_TIMEOUT_0;
	else if (mus == ~0UL)
		return L4_IPC_TIMEOUT_NEVER;

	long e = Genode::log2(mus) - 7;
	unsigned long m;

	if (e < 0) e = 0;
	m = mus / (1UL << e);

	/* check corner case */
	if ((e > 31 ) || (m > 1023)) {
		PWRN("invalid timeout %x, using max. values\n", mus);
		e = 0;
		m = 1023;
	}

	return l4_timeout_rel(m, e);
}


namespace Timer {

	enum { STACK_SIZE = 4096 };

	/**
	 * Timer session
	 */
	class Session_component : public Genode::Rpc_object<Session>,
	                          public Genode::List<Session_component>::Element
	{
		private:

			Genode::Rpc_entrypoint           _entrypoint;
			Session_capability               _session_cap;
			Genode::Attached_rom_dataspace   _kip_ds;
			Fiasco::l4_kernel_info_t * const _kip;
			Fiasco::l4_cpu_time_t const      _initial_clock_value;

		public:

			/**
			 * Constructor
			 */
			Session_component(Genode::Cap_session *cap)
			:
				_entrypoint(cap, STACK_SIZE, "timer_session_ep"),
				_session_cap(_entrypoint.manage(this)),
				_kip_ds("l4v2_kip"),
				_kip(_kip_ds.local_addr<Fiasco::l4_kernel_info_t>()),
				_initial_clock_value(_kip->clock)
			{ }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				_entrypoint.dissolve(this);
			}

			/**
			 * Return true if capability belongs to session object
			 */
			bool belongs_to(Genode::Session_capability cap)
			{
				return _entrypoint.obj_by_cap(cap) == this;
			}

			/**
			 * Return session capability
			 */
			Session_capability cap() { return _session_cap; }


			/*****************************
			 ** Timer session interface **
			 *****************************/

			void msleep(unsigned ms)
			{
				using namespace Fiasco;

				l4_ipc_sleep(l4_timeout(L4_IPC_TIMEOUT_NEVER, mus_to_timeout(1000*ms)));
			}

			unsigned long elapsed_ms() const
			{
				return (_kip->clock - _initial_clock_value) / 1000;
			}
	};
}

#endif
