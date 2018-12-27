/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WINDOW_ELEMENT_H_
#define _WINDOW_ELEMENT_H_

/* Genode includes */
#include <util/lazy_value.h>
#include <util/color.h>

/* gems includes */
#include <gems/animator.h>

/* local includes */
#include "canvas.h"

namespace Decorator { class Window_element; }


class Decorator::Window_element : public Animator::Item
{
	public:

		enum Type { TITLE, LEFT, RIGHT, TOP, BOTTOM,
		            TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
		            CLOSER, MAXIMIZER, MINIMIZER, UNMAXIMIZER, UNDEFINED };

		struct State
		{
			bool  focused;
			bool  highlighted;
			bool  pressed;
			Color base_color;

			bool operator == (State const &other) const
			{
				return focused     == other.focused
				    && highlighted == other.highlighted
				    && pressed     == other.pressed
				    && base_color  == other.base_color;
			}
		};

	private:

		static Color _add(Color c1, Color c2)
		{
			return Color(Genode::min(c1.r + c2.r, 255),
			             Genode::min(c1.g + c2.g, 255),
			             Genode::min(c1.b + c2.b, 255));
		}

		static Color _sub(Color c1, Color c2)
		{
			return Color(Genode::max(c1.r - c2.r, 0),
			             Genode::max(c1.g - c2.g, 0),
			             Genode::max(c1.b - c2.b, 0));
		}

		Type const _type;

		State _state { };

		/*
		 * Color value in 8.4 fixpoint format. We use four bits to
		 * represent the fractional part to enable smooth
		 * interpolation between the color values.
		 */
		Lazy_value<int> _r { }, _g { }, _b { };

		static Color _dst_color(State const &state)
		{
			Color result = state.base_color;

			if (state.focused)
				result = _add(result, Color(70, 70, 70));

			if (state.highlighted)
				result = _add(result, Color(65, 60, 55));

			if (state.pressed)
				result = _sub(result, Color(10, 10, 10));

			return result;
		}

		unsigned _anim_steps(State const &state) const
		{
			/* immediately respond when pressing or releasing an element */
			if (_state.pressed != state.pressed)
				return 0;

			/* medium fade-in when gaining the focus or hover highlight */
			if ((!_state.focused     && state.focused)
			 || (!_state.highlighted && state.highlighted))
				return 15;

			/* slow fade-out when leaving focus or hover highlight */
			return 20;
		}

	public:

		Window_element(Type type, Animator &animator, Color base_color)
		:
			Animator::Item(animator),
			_type(type)
		{
			apply_state(State{ .focused     = false,
			                   .highlighted = false,
			                   .pressed     = false,
			                   .base_color  = base_color });
		}

		Type type() const { return _type; }

		char const *type_name() const
		{
			switch (_type) {
			case UNDEFINED:    return "";
			case TITLE:        return "title";
			case LEFT:         return "left";
			case RIGHT:        return "right";
			case TOP:          return "top";
			case BOTTOM:       return "bottom";
			case TOP_LEFT:     return "top_left";
			case TOP_RIGHT:    return "top_right";
			case BOTTOM_LEFT:  return "bottom_left";
			case BOTTOM_RIGHT: return "bottom_right";
			case CLOSER:       return "closer";
			case MINIMIZER:    return "minimizer";
			case MAXIMIZER:    return "maximizer";
			case UNMAXIMIZER:  return "unmaximizer";
			}
			return "";
		}

		Color color() const { return Color(_r >> 4, _g >> 4, _b >> 4); }

		/**
		 * \return true if state has changed
		 */
		bool apply_state(State state)
		{
			if (_state == state)
				return false;

			Color    const dst_color = _dst_color(state);
			unsigned const steps     = _anim_steps(state);

			_r.dst(dst_color.r << 4, steps);
			_g.dst(dst_color.g << 4, steps);
			_b.dst(dst_color.b << 4, steps);

			/* schedule animation */
			animate();

			_state = state;

			return true;
		}

		State state() const { return _state; }

		bool pressed() const { return _state.pressed; }

		/**
		 * Animator::Item interface
		 */
		void animate() override;
};

#endif /* _WINDOW_ELEMENT_H_ */
