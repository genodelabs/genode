/*
 * \brief  Helper for implementing the fading of colors
 * \author Norman Feske
 * \date   2020-02-19
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ANIMATED_COLOR_H_
#define _ANIMATED_COLOR_H_

/* demo includes */
#include <util/lazy_value.h>
#include <util/color.h>
#include <gems/animator.h>

namespace Genode { class Animated_color; }


class Genode::Animated_color : private Animator::Item, Noncopyable
{
	public:

		struct Steps { unsigned value; };

	private:

		Color _color { };

		using Lazy_value = ::Lazy_value<int>;

		struct Animated_channel
		{
			bool _initial = true;

			Lazy_value _value { };

			Steps _remaining { 0 };

			void animate()
			{
				_value.animate();

				if (_remaining.value > 1)
					_remaining.value--;
			}

			bool animated() const { return _value != _value.dst(); }

			void fade_to(int value, Steps steps)
			{
				if (_initial) {
					_value = Lazy_value(value << 10);
					_initial = false;

				} else {

					/* retarget animation while already in progress */
					if (animated())
						steps.value = max(_remaining.value, 1U);

					_value.dst(value << 10, steps.value);
					_remaining = steps;
				}
			}

			int value() const { return _value >> 10; }
		};

		Animated_channel _r { }, _g { }, _b { }, _a { };

	public:

		Animated_color(Animator &animator) : Animator::Item(animator) { }

		/**
		 * Animator::Item interface
		 */
		void animate() override
		{
			_r.animate(); _g.animate(); _b.animate(); _a.animate();

			_color = Color(_r.value(), _g.value(), _b.value(), _a.value());

			/* schedule / de-schedule animation */
			Animator::Item::animated(_r.animated() || _g.animated() ||
			                         _b.animated() || _a.animated());
		}

		/**
		 * Assign new target color
		 *
		 * The first assignment assigns the color directly without animation.
		 * All subsequent assignments result in an animated transition to the
		 * target color.
		 */
		void fade_to(Color color, Steps steps)
		{
			_r.fade_to(color.r, steps);
			_g.fade_to(color.g, steps);
			_b.fade_to(color.b, steps);
			_a.fade_to(color.a, steps);

			animate();
		}

		bool animated() const { return Animator::Item::animated(); }

		Color color() const { return _color; }
};

#endif /* _ANIMATED_COLOR_H_ */
