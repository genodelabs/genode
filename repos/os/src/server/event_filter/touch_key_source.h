/*
 * \brief  Input-event source that generates press/release from touch events
 * \author Norman Feske
 * \date   2022-08-22
 *
 * This filter generates artificial key press/release event pairs when touching
 * pre-defined areas on a touch screen. All events occurring while such a
 * special area is touched are suppressed.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__TOUCH_KEY_SOURCE_H_
#define _EVENT_FILTER__TOUCH_KEY_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>
#include <util/geometry.h>
#include <base/allocator.h>

/* local includes */
#include <include_accessor.h>
#include <source.h>

namespace Event_filter { class Touch_key_source; }


class Event_filter::Touch_key_source : public Source, Source::Filter
{
	private:

		using Rect = Genode::Rect<>;

		Owner _owner;

		Include_accessor &_include_accessor;

		Source &_source;

		Allocator &_alloc;

		bool _pressed = false; /* true during touch sequence */

		struct Tap : Interface
		{
			Rect const rect;

			Input::Keycode const code;

			static Input::Keycode code_from_node(Node const &node)
			{
				try {
					return key_code_by_name(node.attribute_value("key", Key_name()));
				}
				catch (Unknown_key) { }
				warning("ignoring tap rule ", node);
				return Input::KEY_UNKNOWN;
			}

			Tap(Node const &node)
			: rect(Rect::from_node(node)), code(code_from_node(node)) { }
		};

		Registry<Registered<Tap>> _tap_rules { };

		/**
		 * Filter interface
		 *
		 * The order of incoming events is as follows.
		 *
		 * TOUCH         id x y
		 * PRESS         BTN_TOUCH
		 * TOUCH_RELEASE id
		 * RELEASE       BTN_TOUCH
		 *
		 * Whenever the coordinates x,y match a tap area, this sequence is
		 * replaced by a PRESS-RELEASE with configured key code.
		 */
		void filter_event(Sink &destination, Input::Event const &event) override
		{
			Input::Event ev = event;

			ev.handle_touch([&] (Input::Touch_id id, float x, float y) {

				/* respond to initial touch of first finger only */
				if (id.value != 0 || _pressed)
					return;

				_tap_rules.for_each([&] (Tap const &tap) {
					if (tap.rect.contains(Point((int)(x), (int)(y)))) {
						destination.submit(Input::Press   { tap.code });
						destination.submit(Input::Release { tap.code });
						_pressed = true;
					}
				});
			});

			/* filter out all events during the touch sequence */
			if (!_pressed)
				destination.submit(ev);

			ev.handle_release([&] (Input::Keycode key) {
				if (key == Input::BTN_TOUCH)
					_pressed = false;
			});
		}

		void _apply_sub_node(Node const &node)
		{
			if (node.type() == "tap")
				new (_alloc) Registered<Tap>(_tap_rules, node);
		}

	public:

		static char const *name() { return "touch-key"; }

		Touch_key_source(Owner &owner, Node const &config,
		                 Source::Factory &factory, Allocator &alloc,
		                 Include_accessor &include_accessor)
		:
			Source(owner),
			_owner(factory),
			_include_accessor(include_accessor),
			_source(factory.create_source_for_sub_node(_owner, config)),
			_alloc(alloc)
		{
			_include_accessor.for_each_sub_node(config, name(),
				[&] (Node const &n) { _apply_sub_node(n); });
		}

		~Touch_key_source()
		{
			_tap_rules.for_each([&] (Registered<Tap> &tap) {
				destroy(_alloc, &tap); });
		}

		void generate(Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__TOUCH_KEY_SOURCE_H_*/
