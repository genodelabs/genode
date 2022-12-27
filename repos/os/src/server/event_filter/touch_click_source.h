/*
 * \brief  Input-event source that augments touch events with pointer events
 * \author Norman Feske
 * \date   2021-11-22
 *
 * This filter supplements touch events with absolute motion events and
 * artificial mouse click/release events as understood by regular GUI
 * applications. The original touch events are preserved, which enables
 * touch-aware applications to interpret them.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__TOUCH_CLICK_SOURCE_H_
#define _EVENT_FILTER__TOUCH_CLICK_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include <source.h>

namespace Event_filter { class Touch_click_source; }


class Event_filter::Touch_click_source : public Source, Source::Filter
{
	private:

		Owner _owner;

		Source &_source;

		bool _pressed = false;

		/**
		 * Filter interface
		 */
		void filter_event(Sink &destination, Input::Event const &event) override
		{
			Input::Event ev = event;

			/* forward original event */
			if (!ev.touch_release())
				destination.submit(ev);

			/* supplement mouse click and absolute motion */
			ev.handle_touch([&] (Input::Touch_id id, float x, float y) {

				/* respond to first finger only */
				if (id.value != 0)
					return;

				destination.submit(Input::Absolute_motion{ int(x), int(y) });

				if (!_pressed) {
					destination.submit(Input::Press { Input::BTN_LEFT });
					_pressed = true;
				}
			});

			/* supplement mouse clack */
			ev.handle_touch_release([&] (Input::Touch_id id) {

				if (id.value != 0)
					return;

				if (_pressed) {
					destination.submit(Input::Release { Input::BTN_LEFT });
					_pressed = false;
				}
			});

			/* forward original event */
			if (ev.touch_release())
				destination.submit(ev);
		}

	public:

		static char const *name() { return "touch-click"; }

		Touch_click_source(Owner &owner, Xml_node config, Source::Factory &factory)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config)))
		{ }

		void generate(Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__TOUCH_CLICK_SOURCE_H_*/
