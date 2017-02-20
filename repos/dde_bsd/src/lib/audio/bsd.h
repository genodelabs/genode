/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-16
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BSD_H_
#define _BSD_H_

/* Genode includes */
#include <base/cache.h>
#include <base/env.h>
#include <irq_session/capability.h>

/* local includes */
#include <scheduler.h>

namespace Bsd {

	int probe_drivers(Genode::Env&, Genode::Allocator&);

	void mem_init(Genode::Env&, Genode::Allocator &);
	void irq_init(Genode::Entrypoint&, Genode::Allocator&);
	void timer_init(Genode::Env&);
	void update_time();


	/**************************
	 ** Bus_driver interface **
	 **************************/

	struct Bus_driver
	{
		virtual Genode::Irq_session_capability irq_session() = 0;

		virtual Genode::addr_t alloc(Genode::size_t size, int align) = 0;
		virtual void           free(Genode::addr_t virt, Genode::size_t size) = 0;
		virtual Genode::addr_t virt_to_phys(Genode::addr_t virt) = 0;
		virtual Genode::addr_t phys_to_virt(Genode::addr_t phys) = 0;
	};

}

#endif /* _BSD_H_ */
