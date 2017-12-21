/*
 * \brief  Interface for flushing mapping from a protection domain
 * \author Norman Feske
 * \date   2013-03-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ADDRESS_SPACE_H_
#define _CORE__INCLUDE__ADDRESS_SPACE_H_

#include <base/stdint.h>
#include <base/weak_ptr.h>

namespace Genode { struct Address_space; }

struct Genode::Address_space : private Weak_object<Address_space>,
                               public  Interface
{
	friend class Locked_ptr<Address_space>;

	struct Core_local_addr { addr_t value; };

	/**
	 * Flush memory mappings of virtual address range
	 *
	 * \param virt_addr  start address of range to flush
	 * \param size       size of range in bytes, must be a multiple of page size
	 */
	virtual void flush(addr_t virt_addr, size_t size, Core_local_addr) = 0;

	using Weak_object<Address_space>::weak_ptr;
	using Weak_object<Address_space>::lock_for_destruction;
};

#endif /* _CORE__INCLUDE__ADDRESS_SPACE_H_ */
