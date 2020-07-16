/*
 * \brief  Input-event source that obtains events from input connection
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__INPUT_SOURCE_H_
#define _EVENT_FILTER__INPUT_SOURCE_H_

/* local includes */
#include <source.h>
#include <event_session.h>

namespace Event_filter { class Input_source; }


class Event_filter::Input_source : public Source
{
	private:

		Input_name const _input_name;

		Event_root const &_event_root;

	public:

		static char const *name() { return "input"; }

		Input_source(Owner &owner, Input_name const &input_name,
		             Event_root const &event_root)
		:
			Source(owner), _input_name(input_name), _event_root(event_root)
		{ }

		void generate(Sink &sink) override
		{
			_event_root.for_each_pending_event(_input_name,
			                                   [&] (Input::Event const &event) {
				sink.submit(event);
			});
		}
};

#endif /* _EVENT_FILTER__INPUT_SOURCE_H_ */
