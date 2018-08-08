/*
 * \brief  Zircon platform configuration
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform_session/connection.h>

#include <zx/device.h>
#include <zx/static_resource.h>

ZX::Device::Device(void *component, bool use_platform) :
	_use_platform(use_platform),
	_io_port(nullptr),
	_io_port_count(0),
	_irq(nullptr),
	_irq_count(0),
	_interfaces({false}),
	_hidbus(0),
	_component(component)
{ }

bool ZX::Device::platform()
{
	return _use_platform;
}

void *ZX::Device::component()
{
	return _component;
}

void ZX::Device::set_io_port(ZX::Platform::Io_port *io_port, int count)
{
	_io_port = io_port;
	_io_port_count = count;
}

void ZX::Device::set_irq(ZX::Platform::Irq *irq, int count)
{
	_irq = irq;
	_irq_count = count;
}

void ZX::Device::set_hidbus(hidbus_ifc_t *bus)
{
	_hidbus = bus;
	_interfaces.hidbus = bus != nullptr;
}

bool ZX::Device::hidbus(hidbus_ifc_t **bus)
{
	*bus = _hidbus;
	return _interfaces.hidbus;
}

Genode::Io_port_session_capability ZX::Device::io_port_resource(Genode::uint16_t port)
{
	for (int i = 0; _io_port && i < _io_port_count; i++){
		if (_io_port[i].port == port){
			return ZX::Resource<::Platform::Device_client>::get_component().io_port(_io_port[i].resource);
		}
	}
	return Genode::Io_port_session_capability();
}

Genode::Irq_session_capability ZX::Device::irq_resource(Genode::uint32_t irq)
{
	for (int i = 0; _irq && i < _irq_count; i++){
		if (_irq[i].irq == irq){
			return ZX::Resource<::Platform::Device_client>::get_component().irq(_irq[i].resource);
		}
	}
	return Genode::Irq_session_capability();
}
