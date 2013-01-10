/**
 * \brief  Framebuffer driver interface
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H

#include <dataspace/capability.h>

namespace Framebuffer_drv {

	/**
	 * Return capability for h/w framebuffer dataspace
	 */
	Genode::Dataspace_capability hw_framebuffer();

	/**
	 * Initialize driver
	 */
	int init();
}

#endif
