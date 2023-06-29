/*
 * \brief  Heartbeat monitoring support
 * \author Norman Feske
 * \date   2018-11-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;

namespace {

	/*
	 * Respond to heartbeat requests from the parent
	 */
	struct Heartbeat_handler
	{
		Env &_env;

		void _handle() { _env.parent().heartbeat_response(); }

		Io_signal_handler<Heartbeat_handler> _handler {
			_env.ep(), *this, &Heartbeat_handler::_handle };

		Heartbeat_handler(Env &env) : _env(env)
		{
			_env.parent().heartbeat_sigh(_handler);
		}

		~Heartbeat_handler()
		{
			_env.parent().heartbeat_sigh(Signal_context_capability());
		}
	};
}


static Constructible<Heartbeat_handler> *_heartbeat_handler_ptr = nullptr;


void Genode::init_heartbeat_monitoring(Env &env)
{
	if (_heartbeat_handler_ptr)
		return;

	_heartbeat_handler_ptr = unmanaged_singleton<Constructible<Heartbeat_handler>>();
	_heartbeat_handler_ptr->construct(env);
}
