/*
 * \brief  Genode XML nodes plus local utilities
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <xml_node.h>

using namespace Genode;


Microseconds Genode::read_sec_attr(Xml_node      const  node,
                                   char          const *name,
                                   unsigned long const  default_sec)
{
	unsigned long sec = node.attribute_value(name, 0UL);
	if (!sec) {
		sec = default_sec;
	}
	return Microseconds(sec * 1000 * 1000);
}
