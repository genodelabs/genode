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

#ifndef _INPUT_FILTER__INPUT_SOURCE_H_
#define _INPUT_FILTER__INPUT_SOURCE_H_

/* local includes */
#include <source.h>
#include <connection.h>

namespace Input_filter { class Input_source; }


class Input_filter::Input_source : public Source
{
	private:

		Input_connection &_connection;
		Sink             &_destination;

	public:

		static char const *name() { return "input"; }

		Input_source(Owner &owner, Input_connection &connection, Sink &destination)
		:
			Source(owner), _connection(connection), _destination(destination)
		{ }

		void generate() override
		{
			_connection.for_each_event([&] (Input::Event const &event) {
				_destination.submit_event(event); });
		}
};

#endif /* _INPUT_FILTER__INPUT_SOURCE_H_ */
