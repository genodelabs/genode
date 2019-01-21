/*
 * \brief  Lx env
 * \author Josef Soentgen
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

/* Genode includes */
#include <base/allocator.h>

/* local includes */
#include <lx_kit/scheduler.h>


#define DEBUG_PRINTK     1
#define DEBUG_DEV_DBG    1
#define DEBUG_SCHEDULING 0


namespace Lx
{
	void emul_init(Genode::Env&, Genode::Allocator&);

	void socket_init(Genode::Entrypoint&, Genode::Allocator&);
	void socket_kick();

	void nic_init(Genode::Env&, Genode::Allocator&);

	Genode::Ram_dataspace_capability backend_alloc(Genode::addr_t, Genode::Cache_attribute);
	void backend_free(Genode::Ram_dataspace_capability);

	void get_mac_address(unsigned char *);

	bool open_device();
}

#endif /* _LX_H_ */
