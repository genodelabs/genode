/*
 * \brief  Core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2025-11-27
 *
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pd_session_component.h>

using namespace Core;
using State = Genode::Pd_session::Managing_system_state;


class System_control_component : public Genode::Rpc_object<Pd_session::System_control>,
                                 public Core::System_control
{
	public:

		State system_control(State const &) override;

		Capability<Pd_session::System_control> control_cap(Affinity::Location const) const override;
};


State System_control_component::system_control(State const &s)
{
	seL4_ARM_SMCContext smc_args    { };
	seL4_ARM_SMCContext smc_results { };

	smc_args.x0 = s.r[0];
	smc_args.x1 = s.r[1];
	smc_args.x2 = s.r[2];
	smc_args.x3 = s.r[3];

	seL4_Error err = seL4_ARM_SMC_Call(seL4_CapSMC, &smc_args, &smc_results);

	State ret { };

	if (err == seL4_NoError)
		ret.r[0] = smc_results.x0;
	else
		ret.r[0] = 1; /* failure */

	return ret;
}


static System_control_component &system_instance()
{
	static System_control_component system_component { };
	return system_component;
}


System_control & Core::init_system_control(Runtime &, Allocator &, Rpc_entrypoint &ep)
{
	ep.manage(&system_instance());
	return system_instance();
}


Capability<Pd_session::System_control> System_control_component::control_cap(Affinity::Location const) const
{
	return system_instance().cap();
}


/***************************
 ** Dummy implementations **
 ***************************/

bool Pd_session_component::assign_pci(addr_t, uint16_t) { return true; }


Pd_session::Map_result Pd_session_component::map(Pd_session::Virt_range)
{
	return Map_result::OK;
}
