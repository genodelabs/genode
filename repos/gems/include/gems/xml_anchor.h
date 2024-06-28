/*
 * \brief  Utility for parsing an anchor attribute from an XML node
 * \author Norman Feske
 * \date   2014-08-21
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__XML_ANCHOR_H_
#define _INCLUDE__GEMS__XML_ANCHOR_H_

#include <util/xml_node.h>

class Anchor
{
	public:

		enum Direction { LOW, CENTER, HIGH };

		Direction horizontal = CENTER, vertical = CENTER;

	private:

		using Xml_node = Genode::Xml_node;

		struct Value
		{
			char const *value;
			Direction horizontal, vertical;
		};

		Value const _values[10] = { { "top_left",     LOW,    LOW    },
		                            { "top",          CENTER, LOW    },
		                            { "top_right",    HIGH,   LOW    },
		                            { "left",         LOW,    CENTER },
		                            { "center",       CENTER, CENTER },
		                            { "right",        HIGH,   CENTER },
		                            { "bottom_left",  LOW,    HIGH   },
		                            { "bottom",       CENTER, HIGH   },
		                            { "bottom_right", HIGH,   HIGH   },
		                            { nullptr,        CENTER, CENTER } };

	public:

		Anchor(Xml_node node)
		{
			char const * const attr = "anchor";

			if (!node.has_attribute(attr))
				return;

			auto const v = node.attribute_value(attr, Genode::String<16>());

			for (Value const *value = _values; value->value; value++) {
				if (v == value->value) {
					horizontal = value->horizontal;
					vertical   = value->vertical;
					return;
				}
			}

			Genode::warning("unsupported anchor attribute value");
		}
};

#endif /* _INCLUDE__GEMS__XML_ANCHOR_H_ */
