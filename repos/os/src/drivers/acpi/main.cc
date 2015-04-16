/*
 * \brief  Service and session interface
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 *
 * The 'acpi_drv' provides the 'PCI' after rewriting the IRQ information of PCI
 * devices. For this it uses the 'pci_drv' as a client and forwards the session
 * capability of the 'pci_drv' afterwards
 */

 /*
  * Copyright (C) 2009-2013 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU General Public License version 2.
  */

#include <os/slave.h>
#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <pci_session/client.h>
#include <irq_session/connection.h>
#include <root/component.h>

#include "acpi.h"

/**
 * IRQ service
 */
namespace Irq {

	typedef Genode::Rpc_object<Genode::Typed_root<Genode::Irq_session> > Irq_session;

	/**
	 * Root interface of IRQ service
	 */
	class Root : public Irq_session
	{
		private:

			Genode::Irq_session::Trigger _mode2trigger(unsigned mode)
			{
				enum { EDGE = 0x4, LEVEL = 0xc };

				switch (mode & 0xc) {
					case EDGE:
						return Genode::Irq_session::TRIGGER_EDGE;
					case LEVEL:
						return Genode::Irq_session::TRIGGER_LEVEL;
					default:
						return Genode::Irq_session::TRIGGER_UNCHANGED;
				}
			}

			Genode::Irq_session::Polarity _mode2polarity(unsigned mode)
			{
				using namespace Genode;
				enum { HIGH = 0x1, LOW = 0x3 };

				switch (mode & 0x3) {
					case HIGH:
						return Genode::Irq_session::POLARITY_HIGH;
					case LOW:
						return Genode::Irq_session::POLARITY_LOW;
					default:
						return Genode::Irq_session::POLARITY_UNCHANGED;
				}
			}

		public:

			/**
			 * Remap IRQ number and create IRQ session at parent
			 */
			Genode::Session_capability session(Root::Session_args const &args,
			                                   Genode::Affinity   const &)
			{
				using namespace Genode;

				if (!args.is_valid_string()) throw Root::Invalid_args();

				long irq_number = Arg_string::find_arg(args.string(), "irq_number").long_value(-1);

				/* check for 'MADT' overrides */
				unsigned mode;
				irq_number = Acpi::override(irq_number, &mode);

				/* allocate IRQ at parent*/
				try {
					Irq_connection irq(irq_number, _mode2trigger(mode), _mode2polarity(mode));
					irq.on_destruction(Irq_connection::KEEP_OPEN);
					return irq.cap();
				} catch (...) { throw Root::Unavailable(); }
			}

			/**
			 * Close session at parent
			 */
			void close(Genode::Session_capability session) {
				Genode::env()->parent()->close(session); }

			void upgrade(Genode::Session_capability session, Upgrade_args const &args) { }
	};
}


namespace Pci {

	struct Provider
	{
		bool ready_to_use() { return root().valid(); }

		virtual Genode::Root_capability root() = 0;
	};

	/**
	 * Root interface of PCI service
	 */
	class Root : public Genode::Rpc_object<Genode::Typed_root<Pci::Session> >
	{
		private:

			Provider &_pci_provider;

		public:

			Root(Provider &pci_provider) : _pci_provider(pci_provider) { }

			Genode::Session_capability session(Session_args     const &args,
			                                   Genode::Affinity const &affinity)
			{
				if (!args.is_valid_string()) throw Invalid_args();

				if (!_pci_provider.ready_to_use())
					throw Unavailable();

				try {
					return Genode::Root_client(_pci_provider.root())
					       .session(args.string(), affinity);
				} catch (...) {
					throw Unavailable();
				}
			}

			void close(Genode::Session_capability session) {
				Genode::Root_client(_pci_provider.root()).close(session); }

			void upgrade(Genode::Session_capability s, Upgrade_args const & u)
			{
				try { Genode::Root_client(_pci_provider.root()).upgrade(s, u); }
				catch (...) { throw Invalid_args(); }
			}
	};
}


class Pci_policy : public Genode::Slave_policy, public Pci::Provider
{
	private:

		Genode::Root_capability _cap;
		Genode::Rpc_entrypoint &_pci_ep;
		Genode::Rpc_entrypoint &_irq_ep;
		Genode::Lock _lock;

	protected:

		char const **_permitted_services() const
		{
			static char const *permitted_services[] = { "CPU", "CAP", "RM",
			                                            "LOG", "IO_PORT", "PD",
			                                            "ROM", "RAM", "SIGNAL",
			                                            "IO_MEM", "IRQ", 0 };
			return permitted_services;
		};

		/**
		 * Parse ACPI tables and announce slave PCI service
		 */
		void _acpi_session()
		{
			Pci::Session_capability session;
			const char *args = "label=\"acpi_drv\", ram_quota=16K";

			try {
				using namespace Genode;
				session = static_cap_cast<Pci::Session>(Root_client(_cap)
				          .session(args, Genode::Affinity()));
			} catch (...) { return; }

			Acpi::configure_pci_devices(session);

			/* announce PCI/IRQ services to parent */
			static Pci::Root pci_root(*this);
			static Irq::Root irq_root;

			Genode::env()->parent()->announce(_pci_ep.manage(&pci_root));
			Genode::env()->parent()->announce(_irq_ep.manage(&irq_root));

			Genode::Root_client(_cap).close(session);
		}

	public:

		Pci_policy(Genode::Rpc_entrypoint &slave_ep,
		           Genode::Rpc_entrypoint &pci_ep,
		           Genode::Rpc_entrypoint &irq_ep)
		:
			Slave_policy("pci_drv", slave_ep, Genode::env()->ram_session()),
			 _pci_ep(pci_ep), _irq_ep(irq_ep), _lock(Genode::Lock::LOCKED)
		{ }

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			/* wait for 'pci_drv' to announce the PCI service */
			if (Genode::strcmp(service_name, "PCI"))
				return false;

			_cap = root;

			/* unblock main thread blocked in wait_for_pci_drv */
			_lock.unlock();

			return true;
		}

		void wait_for_pci_drv() {
			/* wait until pci drv is ready */
			_lock.lock();

			/* connect session and start ACPI parsing */
			_acpi_session();
		}

		Genode::Root_capability root() { return _cap; }
};


int main(int argc, char **argv)
{
	using namespace Genode;

	enum { STACK_SIZE = 2*4096, ACPI_MEMORY_SIZE = 2 * 1024 * 1024 };

	/* reserve portion for acpi and give rest to pci_drv */
	Genode::addr_t avail_size = Genode::env()->ram_session()->avail();
	if (avail_size < ACPI_MEMORY_SIZE) {
		PERR("not enough memory");
		return 1;
	}
	avail_size -= ACPI_MEMORY_SIZE;
	PINF("available memory for ACPI %d kiB, for PCI_DRV %lu kiB",
	     ACPI_MEMORY_SIZE / 1024, avail_size / 1024);

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "acpi_ep");

	/* IRQ service */
	static Cap_connection irq_cap;
	static Rpc_entrypoint irq_ep(&irq_cap, STACK_SIZE, "acpi_irq_ep");

	/* use 'pci_drv' as slave service */
	static Rpc_entrypoint pci_ep(&cap, STACK_SIZE, "pci_slave");
	static Pci_policy     pci_policy(pci_ep, ep, irq_ep);

	/* generate config file for pci_drv */
	char buf[1024];
	Acpi::create_pci_config_file(buf, sizeof(buf));
	pci_policy.configure(buf);

	/* use 'pci_drv' as slave service */
	static Genode::Slave  pci_slave(pci_ep, pci_policy, avail_size);

	/* wait until pci drv is online and then make the acpi work */
	pci_policy.wait_for_pci_drv();

	Genode::sleep_forever();
	return 0;
}

