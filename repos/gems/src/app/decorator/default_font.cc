/*
 * \brief  Accessor for the default font
 * \author Norman Feske
 * \date   2015-09-16
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include "canvas.h"


/**
 * Statically linked binary data
 */
extern char _binary_droidsansb10_tff_start[];


/**
 * Return default font
 */
Decorator::Font &Decorator::default_font()
{
	static Font font(_binary_droidsansb10_tff_start);
	return font;
}



