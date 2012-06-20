/*
 * \brief  Client-side ram session interface
 * \author Norman Feske
 * \date   2006-05-31
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RAM_SESSION__CLIENT_H_
#define _INCLUDE__RAM_SESSION__CLIENT_H_

#include <ram_session/capability.h>
#include <ram_session/ram_session.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Ram_session_client : Rpc_client<Ram_session>
	{
		explicit Ram_session_client(Ram_session_capability session)
		: Rpc_client<Ram_session>(session) { }

		Ram_dataspace_capability alloc(size_t size, bool cached = true) {
			return call<Rpc_alloc>(size, cached); }

		void free(Ram_dataspace_capability ds) { call<Rpc_free>(ds); }

		int ref_account(Ram_session_capability ram_session) {
			return call<Rpc_ref_account>(ram_session); }

		int transfer_quota(Ram_session_capability ram_session, size_t amount) {
			return call<Rpc_transfer_quota>(ram_session, amount); }

		size_t quota() { return call<Rpc_quota>(); }

		size_t used() { return call<Rpc_used>(); }
	};
}

#endif /* _INCLUDE__RAM_SESSION__CLIENT_H_ */
