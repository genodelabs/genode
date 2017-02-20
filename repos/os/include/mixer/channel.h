/*
 * \brief   Mixer channel class
 * \author  Josef Soentgen
 * \date    2015-10-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__MIXER__CHANNEL_H_
#define _INCLUDE__MIXER__CHANNEL_H_

#include <util/string.h>
#include <util/xml_node.h>

namespace Mixer {
	struct Channel;
}

struct Mixer::Channel
{
	struct Invalid_channel { };

	typedef Genode::String<128> Label;
	typedef Genode::String<32>  Name;

	typedef enum { INVALID = -1, LEFT, RIGHT, MAX_CHANNELS } Number;
	typedef enum { TYPE_INVALID, INPUT, OUTPUT } Type;
	typedef enum { MIN = 0, MAX = 100 } Volume_level;

	Type   type;
	Number number;
	Label  label;
	int    volume;
	bool   active;
	bool   muted;

	Channel(Genode::Xml_node const &node)
	{
		Genode::String<8> tmp;
		try { node.attribute("type").value(&tmp); }
		catch (...) { throw Invalid_channel(); }

		if      (tmp == "input")  type = INPUT;
		else if (tmp == "output") type = OUTPUT;
		else                      throw Invalid_channel();

		try {
			node.attribute("label").value(&label);
			number = (Channel::Number) node.attribute_value<long>("number", 0);
			volume = node.attribute_value<long>("volume", 0);
			active = node.attribute_value<bool>("active", true);
			muted  = node.attribute_value<bool>("muted", true);
		} catch (...) { throw Invalid_channel(); }
	}
};

#endif /* _INCLUDE__MIXER__CHANNEL_H_ */
