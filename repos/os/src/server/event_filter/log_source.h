/*
 * \brief  Input-event source that logs key events from another source
 * \author Johannes Schlatow
 * \date   2021-04-26
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__LOG_SOURCE_H_
#define _EVENT_FILTER__LOG_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>
#include <util/bit_array.h>

/* local includes */
#include <include_accessor.h>
#include <source.h>
#include <key_code_by_name.h>

namespace Event_filter { class Log_source; }


class Event_filter::Log_source : public Source, Source::Filter
{
	private:

		using Prefix = String<32>;

		Prefix _prefix = "";

		bool _motion = false;

		Owner _owner;

		Source &_source;

		unsigned _event_cnt  = 0;
		int      _key_cnt    = 0;
		int      _finger_cnt = 0;

		Bit_array<64> _fingers { };

		/**
		 * Filter interface
		 */
		void filter_event(Source::Sink &destination, Input::Event const &event) override
		{
			if (_motion || event.press() || event.release()) {
				if (event.press())   ++_key_cnt;
				if (event.release()) --_key_cnt;

				event.handle_touch([&] (Input::Touch_id id, float, float) {
					if(!_fingers.get(id.value, 1).convert<bool>(
						[] (bool used) { return used; },
						[] (auto)      { return true; }
					)) {
						_finger_cnt++;
						(void)_fingers.set(id.value, 1);
					}
				});

				event.handle_touch_release([&] (Input::Touch_id id) {
					if(_fingers.get(id.value, 1).convert<bool>(
						[] (bool used) { return used; },
						[] (auto)      { return false; }
					)) {
						_finger_cnt--;
						(void)_fingers.clear(id.value, 1);
					}
				});

				log(_prefix, "Input event #", _event_cnt++, "\t", event, "\tkey count: ", _key_cnt, "\tfinger count: ", _finger_cnt);
			}

			/* forward event */
			destination.submit(event);
		}

		void _apply_config(Xml_node const &config)
		{
			_prefix = config.attribute_value("prefix", Prefix());
			_motion = config.attribute_value("motion", false);
		}

	public:

		static char const *name() { return "log"; }

		Log_source(Owner &owner, Xml_node const &config, Source::Factory &factory)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source_for_sub_node(_owner, config))
		{
			_apply_config(config);
		}

		void generate(Source::Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__LOG_SOURCE_H_ */
