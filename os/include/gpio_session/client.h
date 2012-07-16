/*
 * \brief  Client-side Gpio session interface
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO_SESSION_H__CLIENT_H_
#define _INCLUDE__GPIO_SESSION_H__CLIENT_H_

#include <gpio_session/capability.h>
#include <base/rpc_client.h>

namespace Gpio {

	struct Session_client : Genode::Rpc_client<Session>
	{
		explicit Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }


		void direction_output(int gpio, bool enable)
		{
			call<Rpc_direction_output>(gpio, enable);
		}

		void direction_input(int gpio)
		{
			call<Rpc_direction_input>(gpio);
		}

		void dataout(int gpio, bool enable)
		{
			call<Rpc_dataout>(gpio, enable);
		}

		int datain(int gpio)
		{
			return call<Rpc_datain>(gpio);
		}

		void debounce_enable(int gpio, bool enable)
		{
			call<Rpc_debounce_enable>(gpio, enable);
		}

		void debouncing_time(int gpio, unsigned int us)
		{
			call<Rpc_debouncing_time>(gpio, us);
		}

		void falling_detect(int gpio, bool enable)
		{
			call<Rpc_falling_detect>(gpio, enable);
		}

		void rising_detect(int gpio, bool enable)
		{
			call<Rpc_rising_detect>(gpio, enable);
		}

		void irq_enable(int gpio, bool enable)
		{
			call<Rpc_irq_enable>(gpio, enable);
		}

		void irq_sigh(Signal_context_capability cap, int gpio)
		{
			call<Rpc_irq_sigh>(cap, gpio);
		}
	};
}

#endif /* _INCLUDE__GPIO_SESSION_H__CLIENT_H_ */