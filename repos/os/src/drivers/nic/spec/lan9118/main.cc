/*
 * \brief  LAN9118 NIC driver
 * \author Norman Feske
 * \date   2011-05-19
 *
 * Note, this driver is only tested on Qemu. At the current stage, it is not
 * expected to function properly on hardware.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <nic/component.h>
#include <os/server.h>
#include <root/component.h>

/* device definitions */
#include <lan9118_defs.h>

/* driver code */
#include <lan9118.h>

namespace Server { struct Main; }

class Root : public Genode::Root_component<Lan9118, Genode::Single_client>
{
	private:

		Server::Entrypoint &_ep;

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
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, tx_buf_size + rx_buf_size + session_size);
				throw Genode::Root::Quota_exceeded();
			}

			return new (Root::md_alloc())
			            Lan9118(LAN9118_PHYS, LAN9118_SIZE, LAN9118_IRQ,
			            tx_buf_size, rx_buf_size,
			            *env()->heap(),
			            *env()->ram_session(),
			            _ep);
		}

	public:

		Root(Server::Entrypoint &ep, Genode::Allocator &md_alloc)
		: Genode::Root_component<Lan9118, Genode::Single_client>(&ep.rpc_ep(), &md_alloc),
			_ep(ep)
		{ }
};

struct Server::Main
{
	Entrypoint &ep;
	::Root      nic_root{ ep, *Genode::env()->heap() };

	Main(Entrypoint &ep) : ep(ep)
	{
		printf("--- LAN9118 NIC driver started ---\n");
		Genode::env()->parent()->announce(ep.manage(nic_root));
	}
};


namespace Server {

	char const *name() { return "nic_ep"; }

	size_t stack_size() { return 2*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Main main(ep);
	}
}

