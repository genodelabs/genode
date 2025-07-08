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
#include <util/list_model.h>

namespace Window_layouter {

	using namespace Genode;

	using Point = Surface_base::Point;
	using Area  = Surface_base::Area;
	using Rect  = Surface_base::Rect;

	struct Window_id
	{
		unsigned value;

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

	using Name = String<64>;

	Name const name;

	static Name name_from_node(Node const &node)
	{
		return node.attribute_value("name", Name());
	}

	static void generate(Generator &g, Rect const &rect)
	{
		g.attribute("xpos",   rect.x1());
		g.attribute("ypos",   rect.y1());
		g.attribute("width",  rect.w());
		g.attribute("height", rect.h());
	}

	struct Drag
	{
		enum class State { IDLE, DRAGGING, SETTLING };

		State     state;
		bool      moving;     /* distiguish moving from resizing */
		Window_id window_id;
		Point     curr_pos;
		Rect      target_rect;

		bool dragging() const { return state == State::DRAGGING; }

		bool moving_at_target_rect(Rect const &rect) const
		{
			return dragging() && rect == target_rect && moving;
		}

		bool moving_window(Window_id id) const
		{
			return dragging() && id == window_id && moving;
		}
	};

	struct Pick
	{
		bool      picked;
		Window_id window_id;
		Rect      orig_geometry;
	};
}

#endif /* _TYPES_H_ */
