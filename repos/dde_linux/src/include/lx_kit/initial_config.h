/*
 * \brief  Lx_kit initial config utility
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2022-03-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__INITIAL_CONFIG_H_
#define _LX_KIT__INITIAL_CONFIG_H_

#include <base/env.h>
#include <base/attached_rom_dataspace.h>

namespace Lx_kit {
	using namespace Genode;

	struct Initial_config;
}


struct Lx_kit::Initial_config
{
	Attached_rom_dataspace rom;

	void _handle_signal() { rom.update(); }

	Initial_config(Genode::Env &env) : rom(env, "config")
	{
		/*
		 * Defer the startup of the USB driver until the first configuration
		 * becomes available. This is needed in scenarios where the configuration
		 * is dynamically generated and supplied to the USB driver via the
		 * report-ROM service.
		 */

		Io_signal_handler<Initial_config> sigh {
			env.ep(), *this, &Initial_config::_handle_signal };

		rom.sigh(sigh);
		_handle_signal();

		while (rom.xml().type() != "config")
			env.ep().wait_and_dispatch_one_io_signal();

		rom.sigh(Signal_context_capability());
	}
};

#endif /* _LX_KIT__INITIAL_CONFIG_H_ */
