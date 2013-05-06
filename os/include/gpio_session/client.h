/*
 * \brief  Client-side Gpio session interface
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Stefan Kalkowski <Stefan.kalkowski@genode-labs.com>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
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

		void direction(Direction d)      { call<Rpc_direction>(d);       }
		void write(bool level)           { call<Rpc_write>(level);       }
		bool read()                      { return call<Rpc_read>();      }
		void debouncing(unsigned int us) { call<Rpc_debouncing>(us);     }
		void irq_type(Irq_type it)       { call<Rpc_irq_type>(it);       }
		void irq_enable(bool enable)     { call<Rpc_irq_enable>(enable); }
		void irq_sigh(Genode::Signal_context_capability cap) {
			call<Rpc_irq_sigh>(cap); }
	};
}

#endif /* _INCLUDE__GPIO_SESSION_H__CLIENT_H_ */
