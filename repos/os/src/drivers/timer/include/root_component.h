/*
 * \brief  Root interface to timer service
 * \author Norman Feske
 * \author Martin Stein
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ROOT_COMPONENT_H_
#define _ROOT_COMPONENT_H_

/* Genode includes */
#include <root/component.h>

/* local includes */
#include <time_source.h>
#include <session_component.h>

namespace Timer { class Root_component; }


class Timer::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Time_source                     _time_source;
		Genode::Alarm_timeout_scheduler _timeout_scheduler;


		/********************
		 ** Root_component **
		 ********************/

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;
			size_t const ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			if (ram_quota < sizeof(Session_component)) {
				throw Insufficient_ram_quota(); }

			return new (md_alloc())
				Session_component(_timeout_scheduler);
		}

	public:

		Root_component(Genode::Env &env, Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_time_source(env), _timeout_scheduler(_time_source)
		{
			_timeout_scheduler._enable();
		}
};

#endif /* _ROOT_COMPONENT_H_ */
