/*
 * \brief  Utilities for direct capability-space manipulation
 * \author Norman Feske
 * \date   2016-07-08
 *
 * This interface is needed by L4Linux.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC__CAPABILITY_SPACE_H_
#define _INCLUDE__FOC__CAPABILITY_SPACE_H_

#include <base/capability.h>
#include <foc/native_capability.h>

namespace Genode { namespace Capability_space {

	/**
	 * Allocate kernel capability selector without associating it with a
	 * Genode capability
	 */
	Fiasco::l4_cap_idx_t alloc_kcap();

	/**
	 * Release kernel capability selector
	 */
	void free_kcap(Fiasco::l4_cap_idx_t);

	/**
	 * Request kernel capability selector associated with Genode capability
	 */
	Fiasco::l4_cap_idx_t kcap(Native_capability);

} }

#endif /* _INCLUDE__FOC__CAPABILITY_SPACE_H_ */
