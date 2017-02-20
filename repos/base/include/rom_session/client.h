/*
 * \brief  Client-side ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ROM_SESSION__CLIENT_H_
#define _INCLUDE__ROM_SESSION__CLIENT_H_

#include <rom_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Rom_session_client; }


struct Genode::Rom_session_client : Rpc_client<Rom_session>
{
	explicit Rom_session_client(Rom_session_capability session)
	: Rpc_client<Rom_session>(session) { }

	Rom_dataspace_capability dataspace() override {
		return call<Rpc_dataspace>(); }

	bool update() override {
		return call<Rpc_update>(); }

	void sigh(Signal_context_capability cap) override {
		call<Rpc_sigh>(cap); }
};

#endif /* _INCLUDE__ROM_SESSION__CLIENT_H_ */
