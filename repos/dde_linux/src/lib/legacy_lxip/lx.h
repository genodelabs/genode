/*
 * \brief  Lx env
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2014-10-17
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_H_
#define _LX_H_

#include <timer/timeout.h>
#include <base/signal.h>

namespace Lx_kit { class Env; }

namespace Lx {

	void nic_client_init(Genode::Env &env,
	                     Genode::Allocator &alloc,
	                     void (*ticker)());

	void timer_init(Genode::Entrypoint  &ep,
	                ::Timer::Connection &timer,
	                Genode::Allocator   &alloc,
	                void (*ticker)());

	void timer_update_jiffies();

	void lxcc_emul_init(Lx_kit::Env &env);
}

extern "C" void lxip_init();

extern "C" void lxip_configure_static(char const *addr,
                                      char const *netmask,
                                      char const *gateway,
                                      char const *nameserver);
extern "C" void lxip_configure_dhcp();
extern "C" void lxip_configure_mtu(unsigned mtu);

extern "C" bool lxip_do_dhcp();

#endif /* _LX_H_ */
