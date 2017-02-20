/*
 * \brief  Core-specific interface to the local capability space
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_CAPABILITY_SPACE_H_
#define _CORE__INCLUDE__CORE_CAPABILITY_SPACE_H_

/* base-internal includes */
#include <base/internal/rpc_obj_key.h>

namespace Genode { class Cap_sel; class Pd_session; }


namespace Genode { namespace Capability_space {

	/**
	 * Create new RPC object capability for the specified entrypoint
	 */
	Native_capability create_rpc_obj_cap(Native_capability ep_cap,
	                                     Pd_session const *,
	                                     Rpc_obj_key);

	Native_capability create_notification_cap(Genode::Cap_sel &notify_cap);
} }

#endif /* _CORE__INCLUDE__CORE_CAPABILITY_SPACE_H_ */
