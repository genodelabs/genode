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

/* driver code */
#include <lan9118.h>

class Root : public Genode::Root_component<Lan9118, Genode::Single_client>
{
	private:

		Genode::Env           & _env;
		Platform::Connection    _platform { _env };
		Platform::Device_client _device   { _platform.device_by_index(0) };

	protected:

		Lan9118 *_create_session(const char *args) override
		{
			using namespace Genode;

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
				throw Genode::Insufficient_ram_quota();
			}

			return new (Root::md_alloc())
				Lan9118(_device.io_mem_dataspace(), _device.irq(),
			            tx_buf_size, rx_buf_size, *md_alloc(), _env);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &md_alloc)
		: Genode::Root_component<Lan9118,
		                         Genode::Single_client>(env.ep(), md_alloc),
		  _env(env) { }
};


void Component::construct(Genode::Env &env)
{
	static Genode::Heap heap(env.ram(), env.rm());
	static Root         nic_root(env, heap);
	Genode::log("--- LAN9118 NIC driver started ---");
	env.parent().announce(env.ep().manage(nic_root));
}
