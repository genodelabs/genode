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
#include <nic/component.h>
#include <platform_session/connection.h>
#include <root/component.h>

/* NIC driver includes */
#include <drivers/nic/mode.h>

/* driver code */
#include <lan9118.h>

using namespace Genode;

class Nic_root : public Root_component<Lan9118, Single_client>
{
	private:

		Env                     &_env;
		Platform::Device_client &_device;


		/********************
		 ** Root_component **
		 ********************/

		Lan9118 *_create_session(const char *args) override
		{
			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
				      "need ", tx_buf_size + rx_buf_size);
				throw Insufficient_ram_quota();
			}

			return new (md_alloc())
				Lan9118(_device.io_mem_dataspace(), _device.irq(),
			            tx_buf_size, rx_buf_size, *md_alloc(), _env);
		}

	public:

		Nic_root(Env                     &env,
		         Allocator               &md_alloc,
		         Platform::Device_client &device)
		:
			Root_component<Lan9118, Genode::Single_client> { env.ep(), md_alloc },
			_env                                           { env },
			_device                                        { device }
		{ }
};

class Main
{
	private:

		Env                          &_env;
		Heap                          _heap          { _env.ram(), _env.rm() };
		Platform::Connection          _platform      { _env };
		Platform::Device_client       _device        { _platform.device_by_index(0) };
		Constructible<Nic_root>       _nic_root      { };
		Constructible<Uplink_client>  _uplink_client { };

	public:

		Main (Env &env)
		:
			_env { env }
		{
			log("--- LAN9118 NIC driver started ---");

			Attached_rom_dataspace config_rom { _env, "config" };
			Nic_driver_mode const mode {
				read_nic_driver_mode(config_rom.xml()) };

			switch (mode) {
			case Nic_driver_mode::NIC_SERVER:

				_nic_root.construct(_env, _heap, _device);
				_env.parent().announce(_env.ep().manage(*_nic_root));
				break;

			case Nic_driver_mode::UPLINK_CLIENT:

				_uplink_client.construct(
					_env, _heap, _device.io_mem_dataspace(), _device.irq());

				break;
			}
		}
};


void Component::construct(Env &env)
{
	static Main main { env };
}
