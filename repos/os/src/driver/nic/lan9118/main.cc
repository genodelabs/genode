/*
 * \brief  LAN9118 NIC driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-05-19
 *
 * Note, this driver is only tested on Qemu. At the current stage, it is not
 * expected to function properly on hardware.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <platform_session/device.h>

/* driver code */
#include <lan9118.h>

using namespace Genode;

class Main
{
	private:

		Env                      &_env;
		Heap                      _heap          { _env.ram(), _env.rm() };
		Platform::Connection      _platform      { _env };
		Platform::Device          _device        { _platform };
		Platform::Device::Mmio<0> _mmio          { _device };
		Platform::Device::Irq     _irq           { _device };
		Uplink_client             _uplink_client { _env, _heap, _mmio, _irq };

	public:

		Main (Env &env) : _env { env } {
			log("--- LAN9118 NIC driver started ---"); }
};


void Component::construct(Env &env)
{
	static Main main { env };
}
