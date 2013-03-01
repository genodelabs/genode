/*
 * \brief  REGULATOR session interface
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2012-02-15
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__REGULATOR_SESSION_H_
#define _INCLUDE__REGULATOR_SESSION__REGULATOR_SESSION_H_

#include <base/signal.h>
#include <session/session.h>

namespace Regulator {
	enum regulator_state {
		STATE_ON,
		STATE_SLEEP,
		STATE_OFF,
		STATE_ERROR,
	};

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Regulator"; }

		virtual ~Session() { }

		virtual bool disable(unsigned regulator_id) = 0;
		virtual bool enable(unsigned regulator_id) = 0;
		virtual bool is_enabled(unsigned regulator_id) = 0;
		virtual enum regulator_state get_state(unsigned regulator_id) = 0;
		virtual bool set_state(unsigned regulator_id, 
			enum regulator_state state) = 0;
		virtual unsigned min_level(unsigned regulator_id) = 0;
		virtual unsigned num_level_steps(unsigned regulator_id) = 0;
		virtual int get_level(unsigned regulator_id) = 0;
		virtual bool set_level(unsigned regulator_id,
			unsigned min, unsigned max) = 0;

		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_enable, bool, enable, unsigned);
		GENODE_RPC(Rpc_disable, bool, disable, unsigned);
		GENODE_RPC(Rpc_is_enabled, bool, is_enabled, unsigned);
		GENODE_RPC(Rpc_get_state, enum regulator_state, get_state, unsigned);
		GENODE_RPC(Rpc_set_state, bool, set_state, unsigned,
			enum regulator_state);
		GENODE_RPC(Rpc_min_level, unsigned, min_level, unsigned);
		GENODE_RPC(Rpc_num_level_steps, unsigned, num_level_steps,
			unsigned);
		GENODE_RPC(Rpc_get_level, int, get_level, unsigned);
		GENODE_RPC(Rpc_set_level, bool, set_level,
			unsigned, unsigned, unsigned);

		GENODE_RPC_INTERFACE(Rpc_enable,
		                     Rpc_disable,
							 Rpc_is_enabled,
							 Rpc_get_state,
							 Rpc_set_state,
							 Rpc_min_level,
							 Rpc_num_level_steps,
							 Rpc_get_level,
							 Rpc_set_level);
	};
}

#endif /* _INCLUDE__REGULATOR_SESSION__REGULATOR_SESSION_H_ */
