/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-16
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
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

namespace Bsd {

	int probe_drivers(Genode::Env&, Genode::Allocator&);

	void mem_init(Genode::Env&, Genode::Allocator &);
	void timer_init(Genode::Env&);
	void update_time();
}

#endif /* _BSD_H_ */
