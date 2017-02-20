/*
 * \brief   String function wrappers for Genode
 * \date    2008-07-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__STRING_H_
#define _INCLUDE__SCOUT__STRING_H_

#include <util/string.h>

namespace Scout {
	using Genode::strlen;
	using Genode::memset;
	using Genode::memcpy;
}

#endif /* _INCLUDE__SCOUT__STRING_H_ */
