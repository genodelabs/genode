/*
 * \brief  SDL input support
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_

/* Genode include */
#include <input/event.h>

namespace Input { struct Handler; }

struct Input::Handler
{
	virtual void event(Input::Event) = 0;
};

void init_input_backend(Input::Handler &);

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_ */
