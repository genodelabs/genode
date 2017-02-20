/*
 * \brief  Capability
 * \author Norman Feske
 * \date   2016-06-16
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/native_capability.h>

using namespace Genode;


Native_capability::Raw Native_capability::raw() const
{
	/*
	 * On seL4, there is no raw data representation of capabilities.
	 */
	return { { 0, 0, 0, 0 } };
}
