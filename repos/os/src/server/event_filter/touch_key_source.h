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
#include <source.h>

namespace Event_filter { class Touch_key_source; }


class Event_filter::Touch_key_source : public Source, Source::Filter
{
	private:

		using Rect = Genode::Rect<>;

		Owner _owner;

		Source &_source;

		Allocator &_alloc;

		bool _pressed = false; /* true during touch sequence */

		struct Tap : Interface
		{
			Rect const rect;

			Input::Keycode const code;

			static Input::Keycode code_from_xml(Xml_node const &node)
			{
				try {
					return key_code_by_name(node.attribute_value("key", Key_name()));
				}
				catch (Unknown_key) { }
				warning("ignoring tap rule ", node);
				return Input::KEY_UNKNOWN;
			}

			Tap(Xml_node const &node)
			: rect(Rect::from_xml(node)), code(code_from_xml(node)) { }
		};

		Registry<Registered<Tap>> _tap_rules { };

		/**
		 * Filter interface
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

			ev.handle_touch_release([&] (Input::Touch_id id) {
				if (id.value == 0)
					_pressed = false;
			});
		}

	public:

		static char const *name() { return "touch-key"; }

		Touch_key_source(Owner &owner, Xml_node config,
		                 Source::Factory &factory, Allocator &alloc)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config))),
			_alloc(alloc)
		{
			config.for_each_sub_node("tap", [&] (Xml_node const &node) {
				new (_alloc) Registered<Tap>(_tap_rules, node); });
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
