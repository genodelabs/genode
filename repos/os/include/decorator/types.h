/*
 * \brief  Basic types used by decorator
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DECORATOR__TYPES_H_
#define _INCLUDE__DECORATOR__TYPES_H_

/* Genode includes */
#include <gui_session/gui_session.h>
#include <util/xml_node.h>
#include <util/color.h>
#include <util/geometry.h>
#include <util/color.h>
#include <util/dirty_rect.h>
#include <util/list_model.h>
#include <util/interface.h>
#include <os/surface.h>

namespace Decorator {

	using Point      = Genode::Surface_base::Point;
	using Area       = Genode::Surface_base::Area;
	using Rect       = Genode::Surface_base::Rect;
	using Dirty_rect = Genode::Dirty_rect<Rect, 3>;

	using Genode::size_t;
	using Genode::Color;
	using Genode::Xml_node;
	using Genode::List_model;
	using Genode::Interface;
}

#endif /* _INCLUDE__DECORATOR__TYPES_H_ */
