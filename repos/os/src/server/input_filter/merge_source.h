/*
 * \brief  Input-event source that merges other sources
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_FILTER__MERGE_SOURCE_H_
#define _INPUT_FILTER__MERGE_SOURCE_H_

/* local includes */
#include <source.h>

namespace Input_filter { class Merge_source; }


class Input_filter::Merge_source : public Source
{
	private:

		Owner _owner;

	public:

		static char const *name() { return "merge"; }

		Merge_source(Owner &owner, Xml_node config, Sink &destination,
		             Source::Factory &factory)
		:
			Source(owner), _owner(factory)
		{
			config.for_each_sub_node([&] (Xml_node node) {
				if (input_node(node))
					factory.create_source(_owner, node, destination); });
		}

		void generate() override
		{
			_owner.for_each([&] (Source &source) { source.generate(); });
		}
};

#endif /* _INPUT_FILTER__REMAP_SOURCE_H_ */
