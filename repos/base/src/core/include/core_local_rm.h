/*
 * \brief  Core-local region map
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_LOCAL_RM_H_
#define _CORE__INCLUDE__CORE_LOCAL_RM_H_

/* Genode includes */
#include <base/local.h>
#include <base/rpc_server.h>

/* core includes */
#include <types.h>

namespace Core { struct Core_local_rm; }


struct Core::Core_local_rm : Local::Constrained_region_map
{
	Rpc_entrypoint &_ep;

	Core_local_rm(Rpc_entrypoint &ep) : _ep(ep) { }

	Result attach(Dataspace_capability, Attach_attr const &) override;
	void   _free(Attachment &) override;
};

#endif /* _CORE__INCLUDE__CORE_LOCAL_RM_H_ */
