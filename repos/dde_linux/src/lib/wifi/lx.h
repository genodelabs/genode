/*
 * \brief  Lx env
 * \author Josef Soentgen
 * \date   2014-10-17
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_H_
#define _LX_H_

/* Genode includes */
#include <os/server.h>

/* local includes */
#include <scheduler.h>


#define DEBUG_COMPLETION 0
#define DEBUG_DMA        0
#define DEBUG_DRIVER     0
#define DEBUG_IRQ        0
#define DEBUG_KREF       0
#define DEBUG_PRINTK     1
#define DEBUG_DEV_DBG    1
#define DEBUG_PCI        0
#define DEBUG_SKB        0
#define DEBUG_SLAB       0
#define DEBUG_TIMER      0
#define DEBUG_THREAD     0
#define DEBUG_TRACE      0
#define DEBUG_MUTEX      0
#define DEBUG_SCHEDULING 0
#define DEBUG_WORK       0


namespace Lx
{
	void timer_init(Server::Entrypoint &);
	void timer_update_jiffies();

	void work_init(Server::Entrypoint &);

	void socket_init(Server::Entrypoint &);

	void nic_init(Server::Entrypoint &);

	void irq_init(Server::Entrypoint &);

	Genode::Ram_dataspace_capability backend_alloc(Genode::addr_t, Genode::Cache_attribute);
	void backend_free(Genode::Ram_dataspace_capability);

	void printf(char const *fmt, ...);
	void debug_printf(int level, char const *fmt, ...);


	void get_mac_address(unsigned char *);
}

#endif /* _LX_H_ */
