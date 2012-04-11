/*
 * \brief  Demo device-driver manager (d3m)
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2010-09-21
 *
 * D3m is a simple device-driver manager for demo purposes. Currently, it copes
 * with the following tasks:
 *
 * - Merge events of input drivers for PS/2 and USB HID
 * - Probe boot device using the ATAPI and USB storage drivers
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/slave.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/env.h>
#include <rom_session/connection.h>
#include <cap_session/connection.h>
#include <nic_session/client.h>

/* local includes */
#include <input_service.h>
#include <nic_service.h>
#include <block_service.h>


class Ipxe_policy : public Genode::Slave_policy, public Nic::Provider
{
	Genode::Root_capability _cap;
	bool                    _announced;
	Genode::Lock            _lock;

	protected:

		char const **_permitted_services() const
		{
			static char const *permitted_services[] = {
				"CAP", "RM", "LOG", "SIGNAL", "Timer", "PCI",
				"IO_MEM", "IO_PORT", "IRQ", 0 };

			return permitted_services;
		};

	public:

		Ipxe_policy(Genode::Rpc_entrypoint &entrypoint)
		:
			Slave_policy("nic_drv", entrypoint),
			_announced(false),
			_lock(Genode::Lock::LOCKED)
		{ }

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			if (Genode::strcmp(service_name, "Nic"))
				return false;

			_cap = root;
			return true;
		}

		/**
		 * Nic::Provider interface
		 */
		Genode::Root_capability root() { return _cap; }
};


struct Ps2_policy : public Genode::Slave_policy
{
	private:

		Input::Source_registry &_input_source_registry;
		Input::Source           _input_source_registry_entry;

	protected:

		char const **_permitted_services() const
		{
			static char const *allowed_services[] = {
				"CAP", "RM", "IO_PORT", "IRQ", "LOG", 0 };

			return allowed_services;
		};

	public:

		Ps2_policy(Genode::Rpc_entrypoint &entrypoint,
		           Input::Source_registry &input_source_registry)
		:
			Genode::Slave_policy("ps2_drv", entrypoint),
			_input_source_registry(input_source_registry)
		{ }

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			if (Genode::strcmp(service_name, "Input"))
				return false;

			_input_source_registry_entry.connect(root);
			_input_source_registry.add_source(&_input_source_registry_entry);
			return true;
		}
};


struct Usb_policy : public Genode::Slave_policy
{
	private:

		Input::Source_registry &_input_source_registry;
		Input::Source           _input_source_registry_entry;

		Block::Driver_registry &_block_driver_registry;
		Block::Driver           _block_driver;

	protected:

		const char **_permitted_services() const
		{
			static const char *permitted_services[] = {
				"CAP", "RM", "IO_PORT", "IO_MEM", "IRQ", "LOG", "PCI",
				"Timer", "SIGNAL", 0 };

			return permitted_services;
		};

	public:

		Usb_policy(Genode::Rpc_entrypoint &entrypoint,
		           Input::Source_registry &input_source_registry,
		           Block::Driver_registry &block_driver_registry,
		           Genode::Ram_session    *ram,
		           char const             *config)
		:
			Genode::Slave_policy("usb_drv", entrypoint, ram),
			_input_source_registry(input_source_registry),
			_block_driver_registry(block_driver_registry)
		{
			configure(config);
		}

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			if (Genode::strcmp(service_name, "Input") == 0) {
				_input_source_registry_entry.connect(root);
				_input_source_registry.add_source(&_input_source_registry_entry);
				return true;
			}

			if (Genode::strcmp(service_name, "Block") == 0) {
				_block_driver.init("usb_drv", root);
				_block_driver_registry.add_driver(&_block_driver);
				return true;
			}

			return false;
		}
};


class Atapi_policy : public Genode::Slave_policy
{
	private:

		Block::Driver_registry &_block_driver_registry;
		Block::Driver           _block_driver;

	protected:

		const char **_permitted_services() const
		{
			static const char *permitted_services[] = {
				"CAP", "RM", "LOG", "SIGNAL", "Timer", "PCI", "IO_MEM",
				"IO_PORT", "IRQ", 0 };

			return permitted_services;
		};

	public:

		Atapi_policy(Genode::Rpc_entrypoint &entrypoint,
		             Block::Driver_registry &block_driver_registry)
		:
			Genode::Slave_policy("atapi_drv", entrypoint),
			_block_driver_registry(block_driver_registry) { }

		bool announce_service(char const              *service_name,
		                      Genode::Root_capability  root,
		                      Genode::Allocator       *alloc,
		                      Genode::Server          *server)
		{
			if (Genode::strcmp(service_name, "Block"))
				return false;

			_block_driver.init(name(), root);
			_block_driver_registry.add_driver(&_block_driver);
			return true;
		}
};


int main(int argc, char **argv)
{
	using namespace Genode;

	enum { STACK_SIZE = 2*4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "d3m_ep");

	/*
	 * Registry of input-event sources
	 *
	 * Upon startup of the USB HID and PS/2 drivers, this registry gets
	 * populated when each of the drivers announces its respective 'Input'
	 * service.
	 */
	static Input::Source_registry input_source_registry;

	/*
	 * Registry for boot block device
	 */
	static Block::Driver_registry block_driver_registry;

	/* create PS/2 driver */
	static Rpc_entrypoint ps2_ep(&cap, STACK_SIZE, "ps2_slave");
	static Ps2_policy     ps2_policy(ps2_ep, input_source_registry);
	static Genode::Slave  ps2_slave(ps2_ep, ps2_policy, 512*1024);

	/* create USB driver */
	char const *config = "<config><hid/><storage/></config>";
	static Rpc_entrypoint usb_ep(&cap, STACK_SIZE, "usb_slave");
	static Usb_policy     usb_policy(usb_ep, input_source_registry,
	                                 block_driver_registry, env()->ram_session(),
	                                 config);
	static Genode::Slave  usb_slave(usb_ep, usb_policy, 3*1024*1024);

	/* create ATAPI driver */
	static Rpc_entrypoint atapi_ep(&cap, STACK_SIZE, "atapi_slave");
	static Atapi_policy   atapi_policy(atapi_ep, block_driver_registry);
	static Genode::Slave  atapi_slave(atapi_ep, atapi_policy, 1024*1024);

	/* initialize input service */
	static Input::Root input_root(&ep, env()->heap(), input_source_registry);
	env()->parent()->announce(ep.manage(&input_root));

	/*
	 * Always announce 'Nic' service, throw 'Unavailable' during session
	 * request if no appropriate driver could be found.
	 */
	static Rpc_entrypoint nic_ep(&cap, STACK_SIZE, "nic_slave");
	static Ipxe_policy    nic_policy(nic_ep);
	static Genode::Slave  nic_slave(nic_ep, nic_policy, 2048 * 1024);

	static Nic::Root nic_root(nic_policy);
	env()->parent()->announce(ep.manage(&nic_root));

	/*
	 * Announce 'Block' service
	 *
	 * We use a distinct entrypoint for the block driver because otherwise, a
	 * long-taking block session request may defer other session requests
	 * (i.e., input session). By using a different entrypoint, the GUI can
	 * start even if the boot-device probing failed completely.
	 */
	{
		static Rpc_entrypoint ep(&cap, STACK_SIZE, "d3m_block_ep");
		static Block::Root block_root(block_driver_registry);
		env()->parent()->announce(ep.manage(&block_root));
	}

	Genode::sleep_forever();
	return 0;
}
