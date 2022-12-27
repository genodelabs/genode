/*
 * \brief  Utilities for XML parsing
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DECORATOR__XML_UTILS_H_
#define _INCLUDE__DECORATOR__XML_UTILS_H_

#include <decorator/types.h>

namespace Decorator { static Color color(Xml_node const &); }


/**
 * Read color attribute from XML node
 */
static inline Genode::Color Decorator::color(Genode::Xml_node const &color)
{
	return color.attribute_value("color", Color(0, 0, 0));
}

#endif /* _INCLUDE__DECORATOR__XML_UTILS_H_ */
