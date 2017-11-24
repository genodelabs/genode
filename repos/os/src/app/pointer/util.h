/*
 * \brief  VirtualBox pointer utilities
 * \author Christian Helmuth
 * \date   2015-06-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POINTER_UTIL_H_
#define _POINTER_UTIL_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/string.h>


namespace Pointer {
	typedef Genode::String<64> String;

	inline String read_string_attribute(Genode::Xml_node const &node,
	                                    char             const *attr,
	                                    String           const &default_value);
}


Pointer::String Pointer::read_string_attribute(Genode::Xml_node const &node,
                                               char             const *attr,
                                               String           const &default_value)
{
	try {
		char buf[String::capacity()];
		node.attribute(attr).value(buf, sizeof(buf));
		return String(Genode::Cstring(buf));
	} catch (...) { return default_value; }
}

#endif /* _POINTER_UTIL_H_ */
