/*
 * \brief  Input-event source that emulates a wheel from motion events
 * \author Norman Feske
 * \date   2017-11-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__BUTTON_SCROLL_SOURCE_H_
#define _EVENT_FILTER__BUTTON_SCROLL_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include <source.h>
#include <key_code_by_name.h>

namespace Event_filter { class Button_scroll_source; }


class Event_filter::Button_scroll_source : public Source, Source::Filter
{
	private:

		struct Wheel
		{
			struct Attr
			{
				Input::Keycode button;

				/**
				 * Factor to scale motion events in percent
				 */
				int factor;

				static Key_name _button_attribute(Node const &node)
				{
					return node.attribute_value("button", Key_name("BTN_MIDDLE"));
				}

				static Attr from_node(Node const &node)
				{
					return {
						.button = key_code_by_name(_button_attribute(node).string()),
						.factor = (int)node.attribute_value("speed_percent", 0L)
					};
				}
			};

			Attr const _attr;

			int const _factor_sign    = (_attr.factor < 0) ? -1 : 1;
			int const _factor_percent = _factor_sign*_attr.factor;

			/**
			 * True while user holds the configured button
			 */
			enum State { IDLE, BUTTON_PRESSED, ACTIVE };

			State _state = IDLE;

			/**
			 * Sum of motion in current direction
			 */
			int _accumulated_motion = 0;

			Wheel(Attr const &attr) : _attr(attr) { }

			void handle_activation(Input::Event const &event)
			{
				switch (_state) {
				case IDLE:
					if (event.key_press(_attr.button)) {
						_state = BUTTON_PRESSED;
						_accumulated_motion = 0;
					}
					break;

				case BUTTON_PRESSED:
					if (event.relative_motion())
						_state = ACTIVE;
					break;

				case ACTIVE:
					break;
				}
			}

			/*
			 * \return true if press/release combination must be delivered
			 *
			 * If the release event follows the press event without
			 * intermediate motion, the press-release combination must be
			 * delivered at release time.
			 */
			bool handle_deactivation(Input::Event const &event)
			{
				if (event.key_release(_attr.button)) {
					bool const emit_press_release = (_state == BUTTON_PRESSED);
					_state = IDLE;
					_accumulated_motion = 0;
					return emit_press_release;
				}

				return false;
			}

			void apply_relative_motion(int motion)
			{
				if (_state != ACTIVE) return;

				/* reset if motion direction changes */
				if (motion*_accumulated_motion < 0)
					_accumulated_motion = 0;

				_accumulated_motion += motion*_factor_percent;
			}

			/**
			 * Return pending wheel motion
			 */
			int pending_motion()
			{
				int const quantizized = _accumulated_motion/100;

				if (quantizized != 0)
					_accumulated_motion -= quantizized*100;

				return _factor_sign*quantizized;
			}

			/**
			 * True if the given event must be filtered out from the event
			 * stream
			 */
			bool suppressed(Input::Event const event)
			{
				return (_state == ACTIVE && event.relative_motion())
				     || event.key_press(_attr.button);
			}

			bool release(Input::Event const event) const
			{
				return event.key_release(_attr.button);
			}
		};

		Wheel _vertical_wheel, _horizontal_wheel;

		Owner _owner;

		Source &_source;

		/**
		 * Filter interface
		 */
		void filter_event(Sink &destination, Input::Event const &event) override
		{
			using Input::Event;

			_vertical_wheel  .handle_activation(event);
			_horizontal_wheel.handle_activation(event);

			event.handle_relative_motion([&] (int x, int y) {
				_vertical_wheel  .apply_relative_motion(y);
				_horizontal_wheel.apply_relative_motion(x);
			});

			/* emit artificial wheel event */
			int const wheel_x = _horizontal_wheel.pending_motion(),
			          wheel_y = _vertical_wheel  .pending_motion();

			if (wheel_x || wheel_y)
				destination.submit(Input::Wheel{wheel_x, wheel_y});

			/*
			 * Submit both press event and release event of magic button at
			 * button-release time.
			 */
			if (_vertical_wheel.release(event) || _horizontal_wheel.release(event)) {

				event.handle_release([&] (Input::Keycode key) {

					/*
					 * Use bitwise or '|' instead of logical or '||' to always
					 * execute both conditions regardless of the result of the
					 * first call of 'handle_activation'.
					 */
					if (_vertical_wheel  .handle_deactivation(event)
					  | _horizontal_wheel.handle_deactivation(event)) {

						destination.submit(Input::Press{key});
						destination.submit(Input::Release{key});
					}
				});
				return;
			}

			/* hide consumed relative motion and magic-button press events */
			if (_vertical_wheel  .suppressed(event)) return;
			if (_horizontal_wheel.suppressed(event)) return;

			destination.submit(event);
		}

		static Wheel::Attr _attr_for_sub_node(Node const &node, char const *type)
		{
			Wheel::Attr result { };
			node.with_optional_sub_node(type, [&] (Node const &sub_node) {
				result = Wheel::Attr::from_node(sub_node); });
			return result;
		}

	public:

		static char const *name() { return "button-scroll"; }

		Button_scroll_source(Owner &owner, Node const &config, Source::Factory &factory)
		:
			Source(owner),
			_vertical_wheel  (_attr_for_sub_node(config, "vertical")),
			_horizontal_wheel(_attr_for_sub_node(config, "horizontal")),
			_owner(factory),
			_source(factory.create_source_for_sub_node(_owner, config))
		{ }

		void generate(Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__BUTTON_SCROLL_SOURCE_H_ */
