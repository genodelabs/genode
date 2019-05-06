/*
 * \brief  State of popup menu
 * \author Norman Feske
 * \date   2018-09-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__POPUP_H_
#define _MODEL__POPUP_H_

#include "types.h"

namespace Sculpt { struct Popup; }


struct Sculpt::Popup : Noncopyable
{
	enum State { OFF, VISIBLE } state { OFF };

	Popup() { }

	Rect anchor { };

	void toggle() { state = (state == OFF) ? VISIBLE : OFF; }
};

#endif /* _MODEL__POPUP_H_ */
