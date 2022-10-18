/*
 * \brief  Interface between kernel and hypervisor
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SPEC__PC__VIRTUALIZATION_HYPERVISOR_H_
#define _SPEC__PC__VIRTUALIZATION_HYPERVISOR_H_

#include <base/stdint.h>
#include <base/log.h>

namespace Hypervisor {

	using Call_arg = Genode::umword_t;
	using Call_ret = Genode::umword_t;

	inline void switch_world(Call_arg guest_state [[maybe_unused]],
	                         Call_arg host_state [[maybe_unused]],
	                         Call_arg pic_state [[maybe_unused]],
	                         Call_arg ttbr [[maybe_unused]])
	{
	}
}

#endif /* _SPEC__PC__VIRTUALIZATION_HYPERVISOR_H_ */
