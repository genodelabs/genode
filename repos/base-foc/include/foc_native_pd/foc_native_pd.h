/*
 * \brief  Fiasco.OC-specific part of the PD session interface
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2011-04-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC_NATIVE_PD__FOC_NATIVE_PD_H_
#define _INCLUDE__FOC_NATIVE_PD__FOC_NATIVE_PD_H_

#include <base/capability.h>
#include <base/rpc.h>
#include <pd_session/pd_session.h>

namespace Genode { struct Foc_native_pd; }


struct Genode::Foc_native_pd : Pd_session::Native_pd
{
	virtual Native_capability task_cap() = 0;

	GENODE_RPC(Rpc_task_cap, Native_capability, task_cap);
	GENODE_RPC_INTERFACE(Rpc_task_cap);
};

#endif /* _INCLUDE__FOC_NATIVE_PD__FOC_NATIVE_PD_H_ */
