/*
 * \brief  Access to pseudo parent capability
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/capability.h>

namespace Genode {

	/**
	 * Return parent capability
	 *
	 * This function is normally provided by the 'startup' library.
	 */
	Native_capability parent_cap() { return Native_capability(); }
}
