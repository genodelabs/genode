/*
 * \brief  Zircon IO port handling
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <io_port_session/connection.h>
#include <platform_session/client.h>

#include <zircon/types.h>

#include <zx/static_resource.h>
#include <zx/device.h>

static Genode::Io_port_session_capability _port_reg[65536] = {};

extern "C" {

	Genode::uint8_t inp(Genode::uint16_t port)
	{
		if (ZX::Resource<ZX::Device>::get_component().platform() && _port_reg[port].valid()){
			return Genode::Io_port_session_client(_port_reg[port]).inb(port);
		}else{
			return ZX::Resource<Genode::Io_port_connection>::get_component().inb(port);
		}
	}

	void outp(Genode::uint16_t port, Genode::uint8_t data)
	{
		if (ZX::Resource<ZX::Device>::get_component().platform() && _port_reg[port].valid()){
			Genode::Io_port_session_client(_port_reg[port]).outb(port, data);
		}else{
			ZX::Resource<Genode::Io_port_connection>::get_component().outb(port, data);
		}
	}

	zx_status_t zx_ioports_request(zx_handle_t,
	                               Genode::uint16_t io_addr,
	                               Genode::uint32_t len)
	{
		ZX::Device &dev = ZX::Resource<ZX::Device>::get_component();
		if (dev.platform()){
			for (Genode::uint32_t i = io_addr; i < io_addr + len && io_addr + len < 65536; i++){
				_port_reg[i] = dev.io_port_resource(i);
				if (!_port_reg[i].valid()){
					Genode::warning("No valid resource available for IO port ", Genode::Hex(i));
					return ZX_ERR_NO_RESOURCES;
				}
			}
		}else{
			if (!ZX::Resource<Genode::Io_port_connection>::initialized()){
				Genode::error("IO_PORT request ", Genode::Hex(io_addr), " x ", len, " not satisfied");
				return ZX_ERR_NO_RESOURCES;
			}
		}
		return ZX_OK;
	}

}
