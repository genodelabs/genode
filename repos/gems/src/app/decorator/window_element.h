/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

	private:

		static Color _add(Color c1, Color c2)
		{
			return Color(Genode::min(c1.r + c2.r, 255),
			             Genode::min(c1.g + c2.g, 255),
			             Genode::min(c1.b + c2.b, 255));
		}

		Type _type;

		/*
		 * Rememeber base color to detect when it changes
		 */
		Color _base_color;

		/*
		 * Color value in 8.4 fixpoint format. We use four bits to
		 * represent the fractional part to enable smooth
		 * interpolation between the color values.
		 */
		Lazy_value<int> _r, _g, _b;

		bool _focused     = false;
		bool _highlighted = false;

		static Color _dst_color(bool focused, bool highlighted, Color base)
		{
			Color result = base;

			if (focused)
				result = _add(result, Color(70, 70, 70));

			if (highlighted)
				result = _add(result, Color(65, 60, 55));

			return result;
		}

		unsigned _anim_steps(bool focused, bool highlighted) const
		{
			/* quick fade-in when gaining the focus or hover highlight */
			if ((!_focused && focused) || (!_highlighted && highlighted))
				return 15;

			/* slow fade-out when leaving focus or hover highlight */
			return 20;
		}

		bool _apply_state(bool focused, bool highlighted, Color base_color)
		{
			_base_color = base_color;

			Color const dst_color = _dst_color(focused, highlighted, base_color);
			unsigned const steps = _anim_steps(focused, highlighted);

			_r.dst(dst_color.r << 4, steps);
			_g.dst(dst_color.g << 4, steps);
			_b.dst(dst_color.b << 4, steps);

			/* schedule animation */
			animate();

			_focused     = focused;
			_highlighted = highlighted;

			return true;
		}

	public:

		Window_element(Type type, Animator &animator, Color base_color)
		:
			Animator::Item(animator),
			_type(type)
		{
			_apply_state(false, false, base_color);
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
		bool apply_state(bool focused, bool highlighted, Color base_color)
		{
			if (_focused == focused && _highlighted == highlighted
			 && base_color == _base_color)
				return false;

			return _apply_state(focused, highlighted, base_color);
		}

		/**
		 * Animator::Item interface
		 */
		void animate() override;
};

#endif /* _WINDOW_ELEMENT_H_ */
