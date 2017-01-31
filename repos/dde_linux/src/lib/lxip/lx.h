/*
 * \brief  Lx env
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2014-10-17
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_H_
#define _LX_H_

#include <base/signal.h>

namespace Lx {

	void nic_client_init(Genode::Env &env,
	                     Genode::Allocator &alloc,
	                     void (*ticker)());
	void timer_init(Genode::Env &env,
	                Genode::Allocator &alloc,
	                void (*ticker)());
	void event_init(Genode::Env &env, void (*ticker)());

	void timer_update_jiffies();
}

extern "C" int lxip_init(char const *address_config);

#endif /* _LX_H_ */
