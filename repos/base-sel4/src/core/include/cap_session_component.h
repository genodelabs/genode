/*
 * \brief  Capability session service
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/allocator.h>

namespace Genode { class Cap_session_component; }

struct Genode::Cap_session_component : Rpc_object<Cap_session>
{
	Cap_session_component(Allocator *md_alloc, const char *args) {}

	void upgrade_ram_quota(size_t ram_quota) { }

	Native_capability alloc(Native_capability ep);

	void free(Native_capability cap);

	static Native_capability alloc(Cap_session_component *session,
	                               Native_capability ep);
};

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
