/**
 * \brief  Block driver session creation
 * \author Sebastian Sumpf
 * \date   2015-09-29
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <block/component.h>
#include <util/xml_node.h>

/* local includes */
#include <ahci.h>


namespace Block {
	class Factory;
	class Root_multiple_clients;
	class Main;
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

		Genode::Env       &_env;
		Genode::Allocator &_alloc;
		Genode::Xml_node   _config;

		Xml_node _lookup_policy(char const *label)
		{
			for (Xml_node policy = _config.sub_node("policy");;
			     policy = policy.next("policy")) {

				char label_buf[64];
				policy.attribute("label").value(label_buf, sizeof(label_buf));

				if (Genode::strcmp(label, label_buf) == 0) {
					return policy;
				}
			}

			throw Xml_node::Nonexistent_sub_node();
		}

		long _device_num(const char *session_label, char *model, char *sn, size_t bufs_len)
		{
			long num = -1;

			try {
				Xml_node policy = _lookup_policy(session_label);

				/* try read device port number attribute */
				try {
					policy.attribute("device").value(&num);
				} catch (...) { }

				/* try read device model and serial number attributes */
				try {
					model[0] = sn[0] = 0;
					policy.attribute("model").value(model, bufs_len);
					policy.attribute("serial").value(sn, bufs_len);
				} catch (...) { }
			} catch (...) { }

			return num;
		}

	protected:

		::Session_component *_create_session(const char *args)
		{
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			/* TODO: build quota check */

			/* Search for configured device */
			char label_buf[64], model_buf[64], sn_buf[64];
			Genode::Arg_string::find_arg(args,
			                             "label").string(label_buf,
			                                             sizeof(label_buf),
			                                             "<unlabeled>");
			long num = _device_num(label_buf, model_buf, sn_buf, sizeof(model_buf));
			/* prefer model/serial routing */
			if ((model_buf[0] != 0) && (sn_buf[0] != 0))
				num = Ahci_driver::device_number(model_buf, sn_buf);

			if (num < 0) {
				PERR("No confguration found for client: %s", label_buf);
				throw Root::Invalid_args();
			}

			if (!Ahci_driver::avail(num)) {
				PERR("Device %ld not available", num);
				throw Root::Unavailable();
			}

			Factory *factory = new (&_alloc) Factory(num);
			return new (&_alloc) ::Session_component(*factory,
			                                         _env.ep(), tx_buf_size);
		}

		void _destroy_session(::Session_component *session)
		{
			Driver_factory &factory = session->factory();
			destroy(&_alloc, session);
			destroy(&_alloc, &factory);
		}

	public:

		Root_multiple_clients(Genode::Env &env, Genode::Allocator &alloc,
		                      Genode::Xml_node config)
		:
			Root_component(&env.ep().rpc_ep(), &alloc),
			_env(env), _alloc(alloc), _config(config)
		{ }

		Genode::Entrypoint &entrypoint() override { return _env.ep(); }

		void announce() override
		{
			_env.parent().announce(_env.ep().manage(*this));
		}
};


struct Block::Main
{
	Genode::Env  &env;
	Genode::Heap  heap { env.ram(), env.rm() };

	Genode::Attached_rom_dataspace config { env, "config" };

	Block::Root_multiple_clients root;

	Main(Genode::Env &env)
	: env(env), root(env, heap, config.xml())
	{
		Genode::log("--- Starting AHCI driver ---");
		bool support_atapi = config.xml().attribute_value("atapi", false);
		Ahci_driver::init(env, heap, root, support_atapi);
	}
};


namespace Component {
	Genode::size_t stack_size()      { return 2 * 1024 * sizeof(long); }
	void construct(Genode::Env &env) { static Block::Main server(env); }
}
