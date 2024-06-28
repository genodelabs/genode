/*
 * \brief  Common libc-internal types
 * \author Norman Feske
 * \date   2019-09-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__TYPES_H_
#define _LIBC__INTERNAL__TYPES_H_

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

namespace Libc {

	using namespace Genode;

	using uint64_t = Genode::uint64_t;
	using Binary_name = String<64>;
}

#endif /* _LIBC__INTERNAL__TYPES_H_ */
