/*
 * \brief  Core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2022-12-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu/cpu_state.h>
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


State System_control_component::system_control(State const &request)
{
	bool const suspend = (request.trapno == State::ACPI_SUSPEND_REQUEST);
	State respond { };

	if (!suspend) {
		/* report failed attempt */
		respond.trapno = 0;
		return respond;
	}

	/*
	 * The trapno/ip/sp registers used below are just convention to transfer
	 * the intended sleep state S0 ... S5. The values are read out by an
	 * ACPI AML component and are of type TYP_SLPx as described in the
	 * ACPI specification, e.g. TYP_SLPa and TYP_SLPb. The values differ
	 * between different PC systems/boards.
	 *
	 * \note trapno/ip/sp registers are chosen because they exist in
	 *       Managing_system_state for x86_32 and x86_64.
	 */
	unsigned const sleep_type_a = request.ip & 0xffu;
	unsigned const sleep_type_b = request.sp & 0xffu;

	respond.trapno = Kernel::suspend((sleep_type_b << 8) | sleep_type_a);

	return respond;
}


static System_control_component &system_instance()
{
	static System_control_component system_component { };
	return system_component;
}


System_control & Core::init_system_control(Allocator &, Rpc_entrypoint &ep)
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


Pd_session::Map_result Pd_session_component::map(Pd_session::Virt_range) { return Map_result::OK; }
