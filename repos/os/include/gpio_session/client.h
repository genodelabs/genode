/*
 * \brief  Client-side Gpio session interface
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Stefan Kalkowski <Stefan.kalkowski@genode-labs.com>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPIO_SESSION_H__CLIENT_H_
#define _INCLUDE__GPIO_SESSION_H__CLIENT_H_

#include <gpio_session/capability.h>
#include <base/rpc_client.h>

namespace Gpio { struct Session_client; }


struct Gpio::Session_client : Genode::Rpc_client<Session>
{
	explicit Session_client(Session_capability session)
	: Genode::Rpc_client<Session>(session) { }

	void direction(Direction d)      override { call<Rpc_direction>(d);         }
	void write(bool level)           override { call<Rpc_write>(level);         }
	bool read()                      override { return call<Rpc_read>();        }
	void debouncing(unsigned int us) override { call<Rpc_debouncing>(us);       }

	Genode::Irq_session_capability irq_session(Irq_type type) override {
		return call<Rpc_irq_session>(type); }
};

#endif /* _INCLUDE__GPIO_SESSION_H__CLIENT_H_ */
