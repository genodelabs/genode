/*
 * \brief  Extension of core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Core */
#include <pd_session_component.h>

using namespace Genode;


bool Pd_session_component::assign_pci(addr_t pci_config_memory, uint16_t bdf)
{
	uint8_t res = Nova::assign_pci(_pd.pd_sel(), pci_config_memory, bdf);
	return res == Nova::NOVA_OK;
}
