/*
 * \brief  Startup USB driver library
 * \author Sebastian Sumpf
 * \date   2013-02-20
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/component.h>

extern void start_usb_driver(Genode::Env &env);


Genode::size_t Component::stack_size() {
	return 4*1024*sizeof(long); }


void Component::construct(Genode::Env &env) { start_usb_driver(env); }
