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


struct Genode::Dataspace : Interface
{
	virtual ~Dataspace() { }

	/**
	 * Request size of dataspace
	 */
	virtual size_t size() = 0;

	/**
	 * Return true if dataspace is writeable
	 */
	virtual bool writeable() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_size,      size_t, size);
	GENODE_RPC(Rpc_writeable, bool,   writeable);

	GENODE_RPC_INTERFACE(Rpc_size, Rpc_writeable);
};

#endif /* _INCLUDE__DATASPACE__DATASPACE_H_ */
