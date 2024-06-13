/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2016-01-13
 *
 * This dummy is used on all kernels with no IOMMU and managing system support.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pd_session_component.h>

using namespace Core;


bool Pd_session_component::assign_pci(addr_t, uint16_t) { return true; }


Pd_session::Map_result Pd_session_component::map(Pd_session::Virt_range)
{
	return Map_result::OK;
}


class System_control_dummy : public System_control
{
	public:

		Capability<Pd_session::System_control> control_cap(Affinity::Location) const override
		{
			return { };
		}
};


System_control & Core::init_system_control(Allocator &, Rpc_entrypoint &)
{
	static System_control_dummy dummy { };
	return dummy;
}
