/*
 * \brief  Interface for creating and destroying windows
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DECORATOR__WINDOW_FACTORY_H_
#define _INCLUDE__DECORATOR__WINDOW_FACTORY_H_

#include <decorator/types.h>

namespace Decorator {
	struct Window_base;
	struct Window_factory_base;
}


struct Decorator::Window_factory_base : Interface
{
	virtual Window_base *create  (Xml_node) = 0;
	virtual void         destroy (Window_base *) = 0;
};

#endif /* _INCLUDE__DECORATOR__WINDOW_FACTORY_H_ */
