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

/* local includes */
#include <include_accessor.h>
#include <source.h>
#include <key_code_by_name.h>

namespace Event_filter { class Log_source; }


class Event_filter::Log_source : public Source, Source::Filter
{
	private:

		typedef String<32> Prefix;

		Prefix _prefix = "";

		bool _motion = false;

		Owner _owner;

		Source &_source;

		unsigned _event_cnt = 0;
		int      _key_cnt   = 0;


		/**
		 * Filter interface
		 */
		void filter_event(Source::Sink &destination, Input::Event const &event) override
		{
			if (_motion || event.press() || event.release()) {
				if (event.press())   ++_key_cnt;
				if (event.release()) --_key_cnt;

				log(_prefix, "Input event #", _event_cnt++, "\t", event, "\tkey count: ", _key_cnt);
			}

			/* forward event */
			destination.submit(event);
		}

		void _apply_config(Xml_node const config)
		{
			_prefix = config.attribute_value("prefix", Prefix());
			_motion = config.attribute_value("motion", false);
		}

	public:

		static char const *name() { return "log"; }

		Log_source(Owner &owner, Xml_node config, Source::Factory &factory)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config)))
		{
			_apply_config(config);
		}

		void generate(Source::Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__LOG_SOURCE_H_ */
