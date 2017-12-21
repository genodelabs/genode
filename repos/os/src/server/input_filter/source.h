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

#ifndef _INPUT_FILTER__SOURCE_H_
#define _INPUT_FILTER__SOURCE_H_

/* Genode includes */
#include <base/registry.h>
#include <input/event.h>

/* local includes */
#include <types.h>

namespace Input_filter { struct Source; }


class Input_filter::Source
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
			    || node.type() == "accelerate";

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

		virtual void generate() = 0;

		struct Owner;

		struct Sink : Interface
		{
			virtual void submit_event(Input::Event const &) = 0;
		};

		struct Factory : Interface
		{
			/*
			 * \throw Invalid_config
			 */
			virtual Source &create_source(Owner &, Xml_node, Sink &) = 0;

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

#endif /* _INPUT_FILTER__SOURCE_H_ */
