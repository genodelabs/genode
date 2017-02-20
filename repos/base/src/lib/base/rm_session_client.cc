/*
 * \brief  Client-side stub for RM session
 * \author Norman Feske
 * \date   2016-04-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <rm_session/client.h>

using namespace Genode;


Rm_session_client::Rm_session_client(Capability<Rm_session> cap)
: Rpc_client<Rm_session>(cap) { }


Capability<Region_map> Rm_session_client::create(size_t size) {
	return call<Rpc_create>(size); }


void Rm_session_client::destroy(Capability<Region_map> cap) {
	call<Rpc_destroy>(cap); }

