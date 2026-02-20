/*
 * \brief  Initialize and handle Terminal session for USB-option
 * \author Sebastian Sumpf
 * \date   2026-02-20
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <genode_c_api/terminal.h>

#include <lx_kit/env.h>

#include "usb_option.h"

class Option;

using namespace Genode;

class Option
{
	private:

		Env &_env;

		Signal_handler<Option> _signal_handler  { _env.ep(), *this,
			&Option::handle_signal };

	public:

		Option(Env &env) : _env(env)
		{
			genode_terminal_init(genode_env_ptr(env),
			                     genode_allocator_ptr(Lx_kit::env().heap),
			                     genode_signal_handler_ptr(_signal_handler));
		}

		void handle_signal()
		{
			lx_option_handle_io();
			Lx_kit::env().scheduler.execute();
		}
};


void lx_option_init(void)
{
	new (Lx_kit::env().heap) Option(Lx_kit::env().env);
}
