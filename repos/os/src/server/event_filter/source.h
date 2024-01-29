/*
 * \brief  Input-event source interface
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__SOURCE_H_
#define _EVENT_FILTER__SOURCE_H_

/* Genode includes */
#include <base/registry.h>
#include <util/xml_node.h>
#include <input/event.h>

/* local includes */
#include <types.h>

namespace Event_filter { struct Source; }


class Event_filter::Source
{
	private:

		Registry<Source>::Element _owner_elem;

	public:

		struct Invalid_config : Exception { };

		Source(Registry<Source> &owner) : _owner_elem(owner, *this) { }

		virtual ~Source() { }

		static bool input_node(Xml_node node)
		{
			return node.type() == "input"
			    || node.type() == "remap"
			    || node.type() == "chargen"
			    || node.type() == "merge"
			    || node.type() == "button-scroll"
			    || node.type() == "accelerate"
			    || node.type() == "log"
			    || node.type() == "transform"
			    || node.type() == "touch-click"
			    || node.type() == "touch-key";

			return false;
		}

		static Xml_node input_sub_node(Xml_node node)
		{
			Xml_node result("<none/>");

			node.for_each_sub_node([&] (Xml_node sub_node) {
				if (input_node(sub_node))
					result = sub_node; });

			if (result.type() != "none")
				return result;

			warning("missing input-source sub node in ", node);
			throw Invalid_config { };
		}

		struct Owner;

		using Sink = Event::Session_client::Batch;

		virtual void generate(Sink &) = 0;

		/**
		 * Interface to time-trigger the generate mechanism independently from
		 * incoming events. This is used for emitting character repeat events.
		 */
		struct Trigger : Interface
		{
			virtual void trigger_generate() = 0;
		};

		struct Filter : Interface
		{
			virtual void filter_event(Sink &, Input::Event const &) = 0;

			static void apply(Sink &destination, Filter &filter, Source &source)
			{
				struct Intermediate_sink : Sink
				{
					Sink   &_destination;
					Filter &_filter;

					void submit(Input::Event const &event) override
					{
						_filter.filter_event(_destination, event);
					}

					Intermediate_sink(Sink &destination, Filter &filter)
					: _destination(destination), _filter(filter) { }

				} intermediate_sink { destination, filter };

				source.generate(intermediate_sink);
			}
		};

		struct Factory : Interface
		{
			/*
			 * \throw Invalid_config
			 */
			virtual Source &create_source(Owner &, Xml_node) = 0;

			virtual void destroy_source(Source &) = 0;
		};

		struct Owner : Registry<Source>
		{
			Factory &_factory;

			Owner(Factory &factory) : _factory(factory) { }

			~Owner()
			{
				for_each([&] (Source &s) { _factory.destroy_source(s); });
			}
		};
};

#endif /* _EVENT_FILTER__SOURCE_H_ */
