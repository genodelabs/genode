/*
 * \brief  SDL input support
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_

#include <input/event.h>

/**
 * Wait for an event, Zzz...zz..
 */
Input::Event wait_for_event();

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__SDL__INPUT_H_ */
