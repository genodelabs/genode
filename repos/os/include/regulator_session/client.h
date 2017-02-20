/*
 * \brief  Client-side regulator session interface
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__CLIENT_H_
#define _INCLUDE__REGULATOR_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <regulator_session/capability.h>

namespace Regulator { struct Session_client; }


struct Regulator::Session_client : public Genode::Rpc_client<Session>
{
	/**
	 * Constructor
	 *
	 * \param session  session capability
	 */
	Session_client(Session_capability session)
	: Genode::Rpc_client<Session>(session) { }


	/*********************************
	 ** Regulator session interface **
	 *********************************/

	void level(unsigned long level) override { call<Rpc_set_level>(level);  }
	unsigned long level()           override { return call<Rpc_level>();    }
	void state(bool enable)         override { call<Rpc_set_state>(enable); }
	bool state()                    override { return call<Rpc_state>();    }
};

#endif /* _INCLUDE__REGULATOR_SESSION__CLIENT_H_ */
