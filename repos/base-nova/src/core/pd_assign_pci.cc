/*
 * \brief  Extension of core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core */
#include <pd_session_component.h>

using namespace Genode;


bool Pd_session_component::assign_pci(addr_t pci_config_memory, uint16_t bdf)
{
	uint8_t res = Nova::NOVA_PD_OOM;
	do {
		res = Nova::assign_pci(_pd.pd_sel(), pci_config_memory, bdf);
	} while (res == Nova::NOVA_PD_OOM &&
	         Nova::NOVA_OK == Pager_object::handle_oom(Pager_object::SRC_CORE_PD,
	                                                   _pd.pd_sel(),
	                                                   "core", "ep",
	                                                   Pager_object::Policy::UPGRADE_CORE_TO_DST));

	return res == Nova::NOVA_OK;
}
