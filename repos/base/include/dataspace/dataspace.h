/*
 * \brief  Dataspace interface
 * \author Norman Feske
 * \date   2006-07-05
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DATASPACE__DATASPACE_H_
#define _INCLUDE__DATASPACE__DATASPACE_H_

#include <base/stdint.h>
#include <base/rpc.h>

namespace Genode { struct Dataspace; }


struct Genode::Dataspace
{
	virtual ~Dataspace() { }

	/**
	 * Request size of dataspace
	 */
	virtual size_t size() = 0;

	/**
	 * Request base address in physical address space
	 */
	virtual addr_t phys_addr() = 0;

	/**
	 * Return true if dataspace is writable
	 */
	virtual bool writable() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_size,      size_t, size);
	GENODE_RPC(Rpc_phys_addr, addr_t, phys_addr);
	GENODE_RPC(Rpc_writable,  bool,   writable);

	GENODE_RPC_INTERFACE(Rpc_size, Rpc_phys_addr, Rpc_writable);
};

#endif /* _INCLUDE__DATASPACE__DATASPACE_H_ */
