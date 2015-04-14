/*
 * \brief  Client-side pd session interface
 * \author Christian Helmuth
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PD_SESSION__CLIENT_H_
#define _INCLUDE__PD_SESSION__CLIENT_H_

#include <pd_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Pd_session_client; }


struct Genode::Pd_session_client : Rpc_client<Pd_session>
{
	explicit Pd_session_client(Pd_session_capability session)
	: Rpc_client<Pd_session>(session) { }

	int bind_thread(Thread_capability thread) override {
		return call<Rpc_bind_thread>(thread); }

	int assign_parent(Parent_capability parent) override {
		return call<Rpc_assign_parent>(parent); }

	/**
	 * Dummy stub for PCI-device assignment operation
	 *
	 * The assign_pci function exists only in the NOVA-specific version of the
	 * PD-session interface. This empty dummy stub merely exists to maintain
	 * API compatibility accross all base platforms so that drivers don't need
	 * to distinguish NOVA from non-NOVA.
	 */
	bool assign_pci(addr_t) { return false; }
};

#endif /* _INCLUDE__PD_SESSION__CLIENT_H_ */
