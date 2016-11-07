/*
 * \brief  Capability
 * \author Norman Feske
 * \date   2016-06-16
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

using namespace Genode;


Native_capability::Raw Native_capability::raw() const
{
	/*
	 * On Linux, we don't pass information as a 'raw' representation to
	 * child components. So this function remains unused. We still need
	 * to provide it to prevent link errors of noux, which relies on this
	 * function for implementing 'fork' (not supported on base-linux).
	 */
	return { { 0, 0, 0, 0 } };
}
