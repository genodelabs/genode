/*
 * \brief  NOVA-specific part of the PD session interface
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA_NATIVE_PD__NOVA_NATIVE_PD_H_
#define _INCLUDE__NOVA_NATIVE_PD__NOVA_NATIVE_PD_H_

#include <base/rpc.h>
#include <pd_session/pd_session.h>

struct Genode::Pd_session::Native_pd : Interface
{
	using Alloc_rpc_cap_result = Pd_session::Alloc_rpc_cap_result;

	/**
	 * Allocate RPC object capability
	 *
	 * \param ep     entry point that will use this capability
	 * \param entry  server-side instruction pointer of the RPC handler
	 * \param mtd    NOVA message transfer descriptor
	 *
	 * \return new RPC object capability
	 */
	virtual Alloc_rpc_cap_result alloc_rpc_cap(Native_capability ep,
	                                           addr_t entry, addr_t mtd) = 0;

	/**
	 * Imprint badge into the portal of the specified RPC capability
	 */
	virtual void imprint_rpc_cap(Native_capability cap, unsigned long badge) = 0;

	GENODE_RPC(Rpc_alloc_rpc_cap, Alloc_rpc_cap_result, alloc_rpc_cap,
	           Native_capability, addr_t, addr_t);
	GENODE_RPC(Rpc_imprint_rpc_cap, void, imprint_rpc_cap,
	           Native_capability, unsigned long);
	GENODE_RPC_INTERFACE(Rpc_alloc_rpc_cap, Rpc_imprint_rpc_cap);
};

#endif /* _INCLUDE__NOVA_NATIVE_PD__NOVA_NATIVE_PD_H_ */
