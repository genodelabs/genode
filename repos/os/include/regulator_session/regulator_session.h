/*
 * \brief  Abstract regulator session interface.
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__REGULATOR_SESSION_H_
#define _INCLUDE__REGULATOR_SESSION__REGULATOR_SESSION_H_

#include <session/session.h>

namespace Regulator { struct Session; }


struct Regulator::Session : public Genode::Session
{
	static const char *service_name() { return "Regulator"; }

	virtual ~Session() { }

	/**
	 * Set regulator specific level
	 */
	virtual void level(unsigned long level) = 0;

	/**
	 * Returns current regulator level
	 */
	virtual unsigned long level() = 0;

	/**
	 * Enable/disable regulator
	 */
	virtual void state(bool enable) = 0;

	/**
	 * Returns whether regulator is enabled or not
	 */
	virtual bool state() = 0;


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_set_level, void, level, unsigned long);
	GENODE_RPC(Rpc_level, unsigned long, level);
	GENODE_RPC(Rpc_set_state, void, state, bool);
	GENODE_RPC(Rpc_state, bool, state);
	GENODE_RPC_INTERFACE(Rpc_set_level, Rpc_level, Rpc_set_state, Rpc_state);
};

#endif /* _INCLUDE__REGULATOR_SESSION__REGULATOR_SESSION_H_ */
