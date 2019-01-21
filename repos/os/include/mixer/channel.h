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

	Type   type   { TYPE_INVALID };
	Number number { INVALID };
	Label  label  { };
	int    volume { MIN };
	bool   active { false };
	bool   muted  { false };

	Channel(Genode::Xml_node const &node)
	{
		typedef Genode::String<8> Type;
		Type const type_name = node.attribute_value("type", Type());

		if      (type_name == "input")  type = INPUT;
		else if (type_name == "output") type = OUTPUT;
		else                            throw Invalid_channel();

		label  = node.attribute_value("label", Label());
		number = (Channel::Number) node.attribute_value("number", 0L);
		volume = node.attribute_value("volume", 0L);
		active = node.attribute_value("active", true);
		muted  = node.attribute_value("muted",  true);
	}
};

#endif /* _INCLUDE__MIXER__CHANNEL_H_ */
