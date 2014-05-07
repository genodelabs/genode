/*
 * \brief  Root interface to timer service
 * \author Norman Feske
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_ROOT_H_
#define _TIMER_ROOT_H_

#include <util/arg_string.h>
#include <base/printf.h>
#include <base/heap.h>
#include <root/component.h>
#include <cap_session/cap_session.h>

#include "timer_session_component.h"


namespace Timer {

	class Root_component : public Genode::Root_component<Session_component>
	{
		private:

			Platform_timer    _platform_timer;
			Timeout_scheduler _timeout_scheduler;

		protected:

			Session_component *_create_session(const char *args)
			{
				Genode::size_t ram_quota = Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);

				if (ram_quota < sizeof(Session_component)) {
					PWRN("Insufficient donated ram_quota (%zd bytes), require %zd bytes",
					     ram_quota, sizeof(Session_component));
				}

				return new (md_alloc())
					Session_component(_timeout_scheduler);
			}

		public:

			/**
			 * Constructor
			 *
			 * The 'cap' argument is not used by the single-threaded server
			 * variant.
			 */
			Root_component(Genode::Rpc_entrypoint         *session_ep,
			               Genode::Allocator              *md_alloc,
			               Genode::Cap_session            *cap)
			:
				Genode::Root_component<Session_component>(session_ep, md_alloc),
				_timeout_scheduler(&_platform_timer, session_ep)
			{ }
	};
}

#endif
