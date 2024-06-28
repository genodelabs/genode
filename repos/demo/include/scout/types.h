/*
 * \brief   Basic types used by scout widgets
 * \date    2013-12-31
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__TYPES_H_
#define _INCLUDE__SCOUT__TYPES_H_

#include <util/geometry.h>

namespace Scout {

	using Point = Genode::Point<>;
	using Area  = Genode::Area<>;
	using Rect  = Genode::Rect<>;

	using size_t  = Genode::size_t;
	using uint8_t = Genode::uint8_t;
}

#endif /* _INCLUDE__SCOUT__TYPES_H_ */
