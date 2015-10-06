/**
 * \brief  Block driver session creation
 * \author Sebastian Sumpf
 * \date   2015-09-29
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <block/component.h>
#include <os/config.h>
#include <os/server.h>
#include <util/xml_node.h>

#include "ahci.h"

namespace Block {
	class Factory;
	class Root_multiple_clients;
}


struct Block::Factory : Driver_factory
{
	long device_num;

	Block::Driver *create()
	{
		return Ahci_driver::claim_port(device_num);
	}

	void destroy(Block::Driver *driver)
	{
		Ahci_driver::free_port(device_num);
	}

	Factory(long device_num) : device_num(device_num) { }
};


class Session_component : public Block::Session_component
{
	public:

		Session_component(Block::Driver_factory     &driver_factory,
		                  Server::Entrypoint       &ep, Genode::size_t buf_size)
		: Block::Session_component(driver_factory, ep, buf_size) { }

		Block::Driver_factory &factory() { return _driver_factory; }
};


class Block::Root_multiple_clients : public Root_component< ::Session_component>,
                                     public Ahci_root
{
	private:

		Server::Entrypoint &_ep;

		long _device_num(const char *session_label)
		{
			long num = -1;

			try {
				using namespace Genode;

				Xml_node policy = Genode::config()->xml_node().sub_node("policy");

				for (;; policy = policy.next("policy")) {
					char label_buf[64];
					policy.attribute("label").value(label_buf, sizeof(label_buf));

					if (Genode::strcmp(session_label, label_buf))
						continue;

					/* read device attribute */
					policy.attribute("device").value(&num);
					break;
				}
			} catch (...) {}

			return num;
		}

	protected:

		::Session_component *_create_session(const char *args)
		{
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			/* TODO: build quota check */

			/* Search for configured device */
			char label_buf[64];
			Genode::Arg_string::find_arg(args,
			                             "label").string(label_buf,
			                                             sizeof(label_buf),
			                                             "<unlabeled>");
			long num = _device_num(label_buf);
			if (num < 0) {
				PERR("No confguration found for client: %s", label_buf);
				throw Root::Invalid_args();
			}

			if (!Ahci_driver::is_avail(num)) {
				PERR("Device %ld not available", num);
				throw Root::Unavailable();
			}

			Factory *factory = new (Genode::env()->heap()) Factory(num);
			return new (md_alloc()) ::Session_component(*factory,
			                                            _ep, tx_buf_size);
		}

		void _destroy_session(::Session_component *session)
		{
			Driver_factory &factory = session->factory();
			destroy(env()->heap(), session);
			destroy(env()->heap(), &factory);
		}

	public:

		Root_multiple_clients(Server::Entrypoint &ep, Allocator *md_alloc)
		: Root_component(&ep.rpc_ep(), md_alloc), _ep(ep) { }

		Server::Entrypoint &entrypoint() override { return _ep; }

		void announce() override
		{
			Genode::env()->parent()->announce(_ep.manage(*this));
		}
};


struct Main
{
	Block::Root_multiple_clients root;

	Main(Server::Entrypoint &ep)
	: root(ep, Genode::env()->heap())
	{
		PINF("--- Starting AHCI driver -> done right .-) --\n");
		Ahci_driver::init(root);
	}
};


namespace Server {
	char const *name()                    { return "ahci_ep"; }
	size_t      stack_size()              { return 2 * 1024 * sizeof(long); }
	void        construct(Entrypoint &ep) { static Main server(ep); }
}
