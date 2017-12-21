/*
 * \brief  SDL input support
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_

/* Genode include */
#include <util/interface.h>
#include <input/event.h>

namespace Input { struct Handler; }

struct Input::Handler : Genode::Interface
{
	virtual void event(Input::Event) = 0;
};

void init_input_backend(Genode::Env &, Input::Handler &);

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_ */
