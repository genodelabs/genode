/*
 * \brief  Layout target
 * \author Norman Feske
 * \date   2018-09-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TARGET_H_
#define _TARGET_H_

/* local includes */
#include <types.h>

namespace Window_layouter { class Target; }


struct Window_layouter::Target : Noncopyable
{
	using Name = String<64>;

	struct Visible { bool value; };

	Name     const name;
	unsigned const layer;
	Rect     const rect;
	bool     const visible;

	Target(Xml_node const &target, Rect rect, Visible visible)
	:
		name (target.attribute_value("name", Name())),
		layer(target.attribute_value("layer", 9999U)),
		rect (rect),
		visible(visible.value)
	{ }

	/* needed to use class as 'Registered<Target>' */
	virtual ~Target() { }
};

#endif /* _TARGET_H_ */
