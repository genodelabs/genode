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

struct Genode::Address_space : Genode::Weak_object<Genode::Address_space>
{
	/**
	 * Flush memory mappings of virtual address range
	 *
	 * \param virt_addr  start address of range to flush
	 * \param size       size of range in bytes, must be a multiple of page size
	 */
	virtual void flush(addr_t virt_addr, size_t size) = 0;
};

#endif /* _CORE__INCLUDE__ADDRESS_SPACE_H_ */
