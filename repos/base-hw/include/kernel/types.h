/*
 * \brief  Public kernel types
 * \author Martin stein
 * \date   2016-03-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KERNEL__TYPES_H_
#define _KERNEL__TYPES_H_

/* base-hw includes */
#include <kernel/types.h>
#include <kernel/interface_support.h>

namespace Kernel
{
	using addr_t    = Genode::addr_t;
	using size_t    = Genode::size_t;
	using capid_t   = Genode::uint16_t;
	using time_t    = Genode::uint64_t;
	using timeout_t = Genode::uint32_t;

	constexpr capid_t cap_id_invalid() { return 0; }
}

#endif /* _KERNEL__TYPES_H_ */
