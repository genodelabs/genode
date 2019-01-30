/*
 * \brief  Pseudo RM session client stub targeting the process-local implementation
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rm_session/client.h>

/* base-internal includes */
#include <base/internal/local_capability.h>

using namespace Genode;


/**
 * Return pointer to locally implemented RM session
 *
 * \throw Local_interface::Non_local_capability
 */
static Rm_session *_local(Capability<Rm_session> cap)
{
	return Local_capability<Rm_session>::deref(cap);
}


Rm_session_client::Rm_session_client(Capability<Rm_session> session)
: Rpc_client<Rm_session>(session) { }


Capability<Region_map> Rm_session_client::create(size_t size) {
	return _local(rpc_cap())->create(size); }


void Rm_session_client::destroy(Capability<Region_map> cap) {
	_local(rpc_cap())->destroy(cap); }
