/*
 * \brief  Fb_nit-internal service interface
 * \author Norman Feske
 * \date   2006-09-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SERVICES_H_
#define _SERVICES_H_

#include <base/component.h>
#include <base/allocator.h>
#include <scout/canvas.h>

#include "elements.h"

extern Scout::Element *window_content();
extern void init_window_content(Genode::Ram_session &, Genode::Region_map &,
                                Genode::Allocator &, Input::Session_component &,
                                unsigned fb_w, unsigned fb_h, bool config_alpha);
extern void init_services(Genode::Env &, Input::Session_component &);
extern void lock_window_content();
extern void unlock_window_content();

#endif
