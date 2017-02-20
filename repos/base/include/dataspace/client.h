/*
 * \brief  Dataspace client interface
 * \author Norman Feske
 * \date   2006-05-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DATASPACE__CLIENT_H_
#define _INCLUDE__DATASPACE__CLIENT_H_

#include <dataspace/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Dataspace_client; }


struct Genode::Dataspace_client : Rpc_client<Dataspace>
{
	explicit Dataspace_client(Dataspace_capability ds)
	: Rpc_client<Dataspace>(ds) { }

	size_t size()      override { return call<Rpc_size>();      }
	addr_t phys_addr() override { return call<Rpc_phys_addr>(); }
	bool   writable()  override { return call<Rpc_writable>();  }
};

#endif /* _INCLUDE__DATASPACE__CLIENT_H_ */
