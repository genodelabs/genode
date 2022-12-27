/*
 * \brief  Linux-specific dataspace client interface
 * \author Norman Feske
 * \date   2006-05-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LINUX_DATASPACE__CLIENT_H_
#define _INCLUDE__LINUX_DATASPACE__CLIENT_H_

#include <dataspace/client.h>
#include <linux_dataspace/linux_dataspace.h>
#include <base/rpc_client.h>
#include <util/string.h>

namespace Genode { struct Linux_dataspace_client; }


struct Genode::Linux_dataspace_client : Rpc_client<Linux_dataspace>
{
	explicit Linux_dataspace_client(Dataspace_capability ds)
	: Rpc_client<Linux_dataspace>(static_cap_cast<Linux_dataspace>(ds)) { }


	/*********************************
	 ** Generic dataspace interface **
	 *********************************/

	size_t size()      override { return call<Rpc_size>();      }
	bool   writeable() override { return call<Rpc_writeable>(); }


	/****************************************
	 ** Linux-specific dataspace interface **
	 ****************************************/

	Filename           fname() override { return call<Rpc_fname>(); }
	Untyped_capability fd()    override { return call<Rpc_fd>(); }
};

#endif /* _INCLUDE__LINUX_DATASPACE__CLIENT_H_ */
