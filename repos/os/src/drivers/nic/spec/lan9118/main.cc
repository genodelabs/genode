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
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <nic/component.h>
#include <root/component.h>

/* device definitions */
#include <lan9118_defs.h>

/* driver code */
#include <lan9118.h>

class Root : public Genode::Root_component<Lan9118, Genode::Single_client>
{
	private:

		Genode::Env &_env;

	protected:

		Lan9118 *_create_session(const char *args)
		{
			using namespace Genode;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(Lan9118));
			if (ram_quota < session_size)
				throw Genode::Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
				      "need ", tx_buf_size + rx_buf_size + session_size);
				throw Genode::Root::Quota_exceeded();
			}

			return new (Root::md_alloc())
			            Lan9118(LAN9118_PHYS, LAN9118_SIZE, LAN9118_IRQ,
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

