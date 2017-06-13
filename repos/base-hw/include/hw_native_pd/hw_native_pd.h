/*
 * \brief  HW-specific part of the PD session interface
 * \author Stefan Kalkowski
 * \date   2017-06-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__HW_NATIVE_PD__HW_NATIVE_PD_H_
#define _INCLUDE__HW_NATIVE_PD__HW_NATIVE_PD_H_

#include <base/rpc.h>
#include <pd_session/pd_session.h>

namespace Genode { struct Hw_native_pd; }


struct Genode::Hw_native_pd : Pd_session::Native_pd
{
	virtual void upgrade_cap_slab() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_upgrade_cap_slab, void, upgrade_cap_slab,
	                 GENODE_TYPE_LIST(Out_of_ram));
	GENODE_RPC_INTERFACE(Rpc_upgrade_cap_slab);
};

#endif /* _INCLUDE__HW_NATIVE_PD__HW_NATIVE_PD_H_ */
