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

#ifndef _ZX_DEVICE_H_
#define _ZX_DEVICE_H_

#include <io_port_session/connection.h>
#include <irq_session/connection.h>

#include <ddk/driver.h>
#include <ddk/protocol/hidbus.h>

namespace ZX
{
	namespace Platform
	{
		struct Io_port;
		struct Irq;
	}
	namespace Interface
	{
		struct Available;
	}
	class Device;
}

struct ZX::Platform::Io_port
{
	Genode::uint16_t port;
	Genode::uint8_t resource;
};

struct ZX::Platform::Irq
{
	Genode::uint32_t irq;
	Genode::uint8_t resource;
};

struct ZX::Interface::Available
{
	bool hidbus;
};

class ZX::Device
{
	private:
		bool _use_platform;
		Platform::Io_port *_io_port;
		int _io_port_count;
		Platform::Irq *_irq;
		int _irq_count;

		ZX::Interface::Available _interfaces;
		hidbus_ifc_t *_hidbus;

		void *_component;

	public:
		Device(void *, bool);
		bool platform();
		void *component();
		void set_io_port(Platform::Io_port *, int);
		void set_irq(Platform::Irq *, int);

		void set_hidbus(hidbus_ifc_t *);
		bool hidbus(hidbus_ifc_t **);

		Genode::Io_port_session_capability io_port_resource(Genode::uint16_t);
		Genode::Irq_session_capability irq_resource(Genode::uint32_t);
};

#endif /* ifndef _ZX_DEVICE_H_ */
