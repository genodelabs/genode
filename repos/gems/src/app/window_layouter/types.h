/*
 * \brief  Window layouter
 * \author Norman Feske
 * \date   2015-12-31
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <os/surface.h>

namespace Window_layouter {

	using namespace Genode;

	using Point = Surface_base::Point;
	using Area  = Surface_base::Area;
	using Rect  = Surface_base::Rect;

	struct Window_id
	{
		unsigned value = 0;

		Window_id() { }
		Window_id(unsigned value) : value(value) { }

		bool valid() const { return value != 0; }

		bool operator != (Window_id const &other) const
		{
			return other.value != value;
		}

		bool operator == (Window_id const &other) const
		{
			return other.value == value;
		}
	};

	class Window;
}

#endif /* _TYPES_H_ */
