/*
 * \brief  Client-side region manager session interface
 * \author Norman Feske
 * \date   2016-04-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RM_SESSION__CLIENT_H_
#define _INCLUDE__RM_SESSION__CLIENT_H_

#include <rm_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Rm_session_client; }


struct Genode::Rm_session_client : Rpc_client<Rm_session>
{
	explicit Rm_session_client(Rm_session_capability);

	Capability<Region_map> create(size_t) override;
	void destroy(Capability<Region_map>) override;
};

#endif /* _INCLUDE__RM_SESSION__CLIENT_H_ */
