/*
 * \brief  Common types used by the terminal
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_ 

/* Genode includes */
#include <util/interface.h>
#include <util/list.h>
#include <base/registry.h>
#include <os/surface.h>
#include <terminal_session/terminal_session.h>

/* terminal includes */
#include <terminal/types.h>

namespace Terminal {
	
	using namespace Genode;

	typedef Surface_base::Rect  Rect;
	typedef Surface_base::Area  Area;
	typedef Surface_base::Point Point;

	struct Character_consumer : Interface
	{
		virtual void consume_character(Character c) = 0;
	};
}

#endif /* _TYPES_H_ */
