/*
 * \brief  Fb_nit-internal service interface
 * \author Norman Feske
 * \date   2006-09-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SERVICES_H_
#define _SERVICES_H_

#include "canvas.h"
#include "elements.h"

extern Element *window_content();
extern void init_services(unsigned fb_w, unsigned fb_h, bool config_alpha);

#endif
