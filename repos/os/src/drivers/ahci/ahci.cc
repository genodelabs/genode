/**
 * \brief  AHCI port controller handling
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <timer_session/connection.h>
#include "ata_driver.h"
#include "atapi_driver.h"


struct Timer_delayer : Mmio::Delayer, Timer::Connection
{
	void usleep(unsigned us) { Timer::Connection::usleep(us); }
};


Mmio::Delayer &Hba::delayer()
{
	static Timer_delayer d;
	return d;
}


struct Ahci
{
	/* read device signature */
	enum Signature {
		ATA_SIG        = 0x101,
		ATAPI_SIG      = 0xeb140101,
		ATAPI_SIG_QEMU = 0xeb140000, /* will be fixed in Qemu */
	};

	Ahci_root     &root;
	Platform::Hba &platform_hba = Platform::init(Hba::delayer());
	Hba            hba          = { platform_hba };

	enum { MAX_PORTS = 32 };
	Port_driver   *ports[MAX_PORTS];
	bool           port_claimed[MAX_PORTS];

	Signal_rpc_member<Ahci> irq;
	Signal_rpc_member<Ahci> device_ready;
	unsigned                ready_count = 0;
	bool                    enable_atapi;

	Ahci(Ahci_root &root, bool support_atapi)
	: root(root), irq(root.entrypoint(), *this, &Ahci::handle_irq),
	  device_ready(root.entrypoint(), *this, &Ahci::ready),
	  enable_atapi(support_atapi)
	{
		info();

		/* register irq handler */
		platform_hba.sigh_irq(irq);

		/* initialize HBA (IRQs, memory) */
		hba.init();

		/* search for devices */
		scan_ports();
	}

	bool atapi(unsigned sig)
	{
		return enable_atapi && (sig == ATAPI_SIG_QEMU || sig == ATAPI_SIG);
	}

	bool ata(unsigned sig)
	{
		return sig == ATA_SIG;
	}

	/**
	 * Forward IRQs to ports
	 */
	void handle_irq(unsigned)
	{
		unsigned port_list = hba.read<Hba::Is>();
		while (port_list) {
			unsigned port = log2(port_list);
			port_list    &= ~(1U << port);

			ports[port]->handle_irq();
		}

		/* clear status register */
		hba.ack_irq();

		/* ack at interrupt controller */
		platform_hba.ack_irq();
	}

	void ready(unsigned cnt)
	{
		ready_count -= cnt;
		if (ready_count)
			return;

		/* announce service */
		root.announce();
	}


	void info()
	{
		PINF("\tversion: %x.%04x", hba.read<Hba::Version::Major>(),
		                           hba.read<Hba::Version::Minor>());
		PINF("\tcommand slots: %u", hba.command_slots());
		PINF("\tnative command queuing: %s", hba.ncq() ? "yes" : "no");
		PINF("\t64 bit support: %s", hba.supports_64bit() ? "yes" : "no");
	}

	void scan_ports()
	{
		PINF("\tnumber of ports: %u pi: %x", hba.port_count(), hba.read<Hba::Pi>());
		unsigned available = hba.read<Hba::Pi>();
		for (unsigned i = 0; i < hba.port_count(); i++) {

			/* check if port is implemented */
			if (!(available & (1U << i)))
				continue;

			Port port(hba, platform_hba, i);

			/* check for ATA/ATAPI devices */
			unsigned sig = port.read<Port::Sig>();
			if (!atapi(sig) && !ata(sig)) {
				PINF("\t\t#%u: off", i);
				continue;
			}

			port.reset();

			bool enabled = false;
			try { enabled = port.enable(); }
			catch (Port::Not_ready) { PERR("Could not enable port %u", i); }

			PINF("\t\t#%u: %s", i, atapi(sig) ? "ATAPI" : "ATA");

			if (!enabled)
				continue;


			switch (sig) {

				case ATA_SIG:
					ports[i] = new (Genode::env()->heap()) Ata_driver(port, device_ready);
					ready_count++;
					break;

				case ATAPI_SIG:
				case ATAPI_SIG_QEMU:
					ports[i] = new (Genode::env()->heap()) Atapi_driver(port, device_ready);
					ready_count++;
					break;

				default:
					PWRN("Device signature %x unsupported", sig);
			}
		}
	};

	Block::Driver *claim_port(unsigned port_num)
	{
		if (!avail(port_num))
			throw -1;

		port_claimed[port_num] = true;
		return ports[port_num];
	}

	void free_port(unsigned port_num)
	{
		port_claimed[port_num] = false;
	}

	bool avail(unsigned port_num)
	{
		return port_num < MAX_PORTS && ports[port_num] && !port_claimed[port_num] &&
		       ports[port_num]->ready();
	}

	long device_number(const char *model_num, const char *serial_num)
	{
		for (long port_num = 0; port_num < MAX_PORTS; port_num++) {
			Ata_driver* drv = dynamic_cast<Ata_driver *>(ports[port_num]);
			if (!drv)
				continue;

			if (*drv->model == model_num && *drv->serial == serial_num)
				return port_num;
		}
		return -1;
	}
};


static Ahci *sata_ahci(Ahci *ahci = 0)
{
	static Ahci *a = ahci;
	return a;
}


void Ahci_driver::init(Ahci_root &root, bool support_atapi)
{
	static Ahci ahci(root, support_atapi);
	sata_ahci(&ahci);
}


Block::Driver *Ahci_driver::claim_port(long device_num)
{
	return sata_ahci()->claim_port(device_num);
}


void Ahci_driver::free_port(long device_num)
{
	sata_ahci()->free_port(device_num);
}


bool Ahci_driver::avail(long device_num)
{
	return sata_ahci()->avail(device_num);
}


long Ahci_driver::device_number(char const *model_num, char const *serial_num)
{
	return sata_ahci()->device_number(model_num, serial_num);
}


