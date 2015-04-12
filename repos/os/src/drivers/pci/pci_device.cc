/*
 * \brief  PCI device component implementation
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "pci_session_component.h"
#include "pci_device_component.h"

Genode::Io_port_session_capability Pci::Device_component::io_port(Genode::uint8_t v_id)
{
	Genode::uint8_t max = sizeof(_io_port_conn) / sizeof(_io_port_conn[0]);
	Genode::uint8_t i = 0, r_id = 0;

	for (Resource res = resource(0); i < max; i++, res = resource(i))
	{
		if (res.type() != Resource::IO)
			continue;

		if (v_id != r_id) {
			r_id ++;
			continue;
		}

		if (_io_port_conn[v_id] == nullptr)
			_io_port_conn[v_id] = new (_slab_ioport) Genode::Io_port_connection(res.base(), res.size());

		return _io_port_conn[v_id]->cap();
	}

	return Genode::Io_port_session_capability();
}

Genode::Io_mem_session_capability Pci::Device_component::io_mem(Genode::uint8_t v_id)
{
	Genode::uint8_t max = sizeof(_io_mem_conn) / sizeof(_io_mem_conn[0]);
	Genode::uint8_t i = 0, r_id = 0;

	for (Resource res = resource(0); i < max; i++, res = resource(i))
	{
		if (res.type() != Resource::MEMORY)
			continue;

		if (v_id != r_id) {
			r_id ++;
			continue;
		}

		if (_io_mem_conn[v_id] == nullptr)
			_io_mem_conn[v_id] = new (_slab_iomem) Genode::Io_mem_connection(res.base(), res.size());

		return _io_mem_conn[v_id]->cap();
	}

	return Genode::Io_mem_session_capability();
}
