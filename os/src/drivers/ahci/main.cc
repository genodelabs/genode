/*
 * \brief  Minimal AHCI-ATA driver
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2011-08-10
 *
 * This driver currently supports only one command slot, one FIS, and one PRD
 * per FIS, thus limiting the request size to 4MB per request. Since the packet
 * interface currently only supports a synchronous mode of operation the above
 * limitations seems reasonable.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/sleep.h>
#include <block/driver.h>
#include <block/component.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <irq_session/connection.h>
#include <pci_session/connection.h>
#include <pci_device/client.h>
#include <io_mem_session/connection.h>
#include <timer_session/connection.h>


/**
 * Enable for debugging output
 */
static const bool verbose = false;

using namespace Genode;


/**
 * Base class for register access
 */
class Reg_base
{
	protected:

		uint32_t _value(uint32_t offset)             { return *(volatile uint32_t *)(this + offset); }
		void     _set(uint32_t offset, uint32_t val) { *(volatile uint32_t *)(this + offset) = val; }
};


/*
 * HBA: Generic Host Control
 */
class Generic_ctrl : public Reg_base
{
	public:

		/* host capabilities */
		uint32_t hba_cap()              { return _value(0x0); }

		/* return port count from hba_cap */
		uint32_t port_count()           { return (hba_cap() & 0x1f) + 1; }

		/* return command slot count from hba_cap */
		uint32_t cmd_slots()            { return ((hba_cap() >> 8) & 0x1f) + 1; }

		/* global host control */
		uint32_t hba_ctrl()             { return _value(0x4); }
		void     hba_ctrl(uint32_t val) { _set(0x4, val); }

		/* set Interrupt Enable (IE) in hba_ctrl */
		void global_interrupt_enable()
		{
			hba_ctrl(hba_ctrl() | 2);

			if (verbose)
				PDBG("HBA %x", hba_ctrl());
		}

		/* set AHCI enable (AE) in hba_ctrl */
		void global_enable_ahci()
		{
			if (!(hba_ctrl() & (1 << 31)))
				hba_ctrl(hba_ctrl() | (1 << 31));

			if (verbose)
				PDBG("AHCI ENABLED: %x", hba_ctrl());
		}

		/* global interrupt status (contains port interrupts) */
		uint32_t hba_intr_status()             { return _value(0x8); }
		void     hba_intr_status(uint32_t val) { return _set(0x8, val); }

		/* acknowledge global interrupt */
		void hba_interrupt_ack() { hba_intr_status(hba_intr_status()); }

		/* AHCI version */
		uint32_t version() { return _value(0x10); }
};


/*
 * AHCI port registers (one set per port)
 */
class Ahci_port : public Reg_base
{
	public:

		/* command list base (lower 32 bit) */
		void cmd_list_base(addr_t cmd_base) { _set(0x0, cmd_base); }

		/* receive FIS base address (lower 32 bit) */
		void fis_base(addr_t fis_base) { _set(0x8, fis_base); }

		/* interrupt status */
		uint32_t intr_status()             { return _value(0x10); }
		void     intr_status(uint32_t val) { _set(0x10, val); }

		/* interrupt enable */
		void intr_enable(uint32_t val) { _set(0x14, val); }

		/* command */
		uint32_t cmd()             { return _value(0x18); }
		void     cmd(uint32_t val) { _set(0x18, val); }

		/* task file data */
		uint32_t tfd() { return _value(0x20); }

		/* Serial ATA status */
		uint32_t status() { return _value(0x28); }

		/* Serial ATA control */
		void     sctl(uint32_t val) { _set(0x2c, val); }
		uint32_t sctl()             { return _value(0x2c); }

		/* Serial ATA error */
		void     err(uint32_t val) { _set(0x30, val); }
		uint32_t err()             { return _value(0x30); }

		/* command issue (1 bit per command slot) */
		void     cmd_issue(uint32_t val) { _set(0x38, val); }
		uint32_t cmd_issue()             { return _value(0x38); }

		/**
		 * Check if device is active
		 */
		bool status_active()
		{
			enum {
				PRESENT_ESTABLISHED = 0x3,   /* device is present and connection is established */
				PM_ACTIVE           = 0x100, /* interface is in active power-mngmt. state */
				PM_PARTIRL          = 0x200, /* interface is in partial power-mngmt. state */
				PM_SLUMBER          = 0x600, /* interface is in slumber power-mngmt. state */
			};

			uint32_t stat = status();
			uint32_t pm_stat = stat & 0xf00;

			/* if controller is in sleep state, try to wake up */
			if (pm_stat == PM_PARTIRL || pm_stat == PM_SLUMBER) {

				if (verbose)
					PDBG("Controller is in sleep state, trying to wake up ...");

				cmd(cmd() | (1 << 28));

				while (!(stat & PM_ACTIVE) || (stat & 0xf) != PRESENT_ESTABLISHED)  { stat = status(); }
			}
			return (((stat & 0xf) == PRESENT_ESTABLISHED) && (stat & PM_ACTIVE));
		}

		/**
		 * Enable CMD.ST to start command list processing
		 */
		void hba_enable()
		{
			enum {
				STS_BSY = 0x80, /* device is busy */
				STS_DRQ = 0x08, /* data transfer requested */
			};

			while (tfd() & (STS_BSY | STS_DRQ))
				if (verbose)
					PDBG("TFD %x", tfd());

			cmd(cmd() | 1);
		}

		/**
		 * Disable CMD.ST
		 */
		void hba_disable()
		{
			if ((cmd() & 1) && !(cmd_issue() & 1))
				cmd(cmd() & ~1);
		}

		/**
		 * Enable port interrupts
		 */
		void interrupt_enable() { intr_enable(~0U); }

		/**
		 * Acknowledge port interrupts
		 */
		uint32_t interrupt_ack()
		{
			interrupt_pm_ack();

			uint32_t status = intr_status();
			intr_status(status);
			return status;
		}

		/**
		 * Check and handle interrupts due to power mgmt. state transitions
		 */
		void interrupt_pm_ack()
		{
			enum {
				INT_PORT_CON_STATUS = 0x40,
				INT_PHY_RDY_STATUS =  0x400000,
			};
			uint32_t status = intr_status();

			if (status & INT_PORT_CON_STATUS)

				/* clear DIAG.x */
				err(err() & ~(1 << 26));

			if (status & INT_PORT_CON_STATUS)

				/* clear DIAG.n */
				err(err() & ~(1 << 16));
		}

		/**
		 * Disable power mgmt. set SCTL.IPM to 3
		 */
		void disable_pm() { sctl(sctl() | (3 << 8)); }

		/**
		 * Power up device
		 */
		void get_ready()
		{
			enum {
				SPIN_UP_DEVICE  = 0x2,
				POWER_ON_DEVICE = 0x4,
				FIS_RECV_ENABLE = 0x10,
				ENABLE = SPIN_UP_DEVICE | POWER_ON_DEVICE | FIS_RECV_ENABLE
			};
			cmd(cmd() | ENABLE);
		}

		/**
		 * Reset this port
		 */
		void reset()
		{
			/* check for ST bit in command register */
			if (cmd() & 1)
				PWRN("CMD.ST bit set during device reset --> unknown behavior");

			/* set device initialization bit for at least 1ms */
			sctl((sctl() & ~0xf) | 1);

			Timer::Connection timer;
			timer.msleep(1);

			sctl(sctl() & ~0xf);

			while ((status() & 0xf) != 0x3) {}
		}

		/**
		 * Return the size of this structure
		 */
		static uint32_t size() { return 0x80; }
};


/**
 * AHCI command list structure
 */
struct Command_list
{
	uint8_t  cfl:5;            /* Command FIS length */
	uint8_t  a:1;              /* ATAPI command flag */
	uint8_t  w:1;              /* Write flag */
	uint8_t  p:1;              /* Prefetchable flag */
	uint8_t  unsused;          /* we don't use byte 2 yet */
	uint16_t prdtl;            /* Physical region descr. length */
	uint32_t prdbc;            /* PRD byte count */
	uint32_t cmd_table_base_l; /* Command table base addr (low) */
	uint32_t cmd_table_base_u;
	uint32_t reserved[0];
};


/**
 * AHCI command table structure
 */
struct Command_table
{
	/**
	 * Setup FIS and PRD
	 */
	void setup_command(uint8_t cmd, uint32_t lba48, uint16_t blk_cnt, addr_t phys_addr)
	{
		enum { MAX_BYTES = 1 << 22 }; /* 4MB = one PRD */
		uint8_t *fis = (uint8_t *)this;

		/* setup FIS */
		fis[0] = 0x27;                   /* type = host to device */
		fis[1] = 0x80;                   /* set update command flag */
		fis[2] = cmd;                    /* actual command */
		fis[4] = lba48 & 0xff;           /* LBA 0 - 7 */
		fis[5] = (lba48 >> 8) & 0xff;    /* LBA 8 - 15 */
		fis[6] = (lba48 >> 16) & 0xff;   /* LBA 16 - 23 */
		fis[7] = 0x40;                   /* LBA mode flag */
		fis[8] = (lba48 >> 24) & 0xff;   /* LBA 24 - 31 */
		fis[9] =  0x0;                   /* LBA 32 - 39 */
		fis[10] = 0x0;                   /* LBA 40 - 47 */
		fis[12] = blk_cnt & 0xff;        /* sector count 0 - 7 */
		fis[13] = (blk_cnt >> 8) & 0xff; /* sector count 8 - 15 */

		/* setup PRD for DMA */
		memcpy(&fis[0x80], &phys_addr, 4); /* DBA: data base address */
		uint32_t bytes  = (blk_cnt * 512) - 1;

		if (bytes + 1 > MAX_BYTES) {
			PERR("Unsupported request size %u > %u", bytes, MAX_BYTES);
			throw Block::Driver::Io_error();
		}

		/* set byte count for PRD 22 bit */
		fis[0x8c] = bytes & 0xff;
		fis[0x8d] = (bytes >> 8) & 0xff;
		fis[0x8e] = (bytes >> 16) & 0x3f;
	}
};


/**
 * AHCI device
 */
class Ahci_device
{
	private:

		enum Pci_config {
			PCI_CFG_BMIBA_OFF   = 0x24, /* offset in PCI config space */
			CLASS_MASS_STORAGE  = 0x10000U,
			SUBCLASS_AHCI       = 0x0600U,
			CLASS_MASK          = 0xffff00U,
			AHCI_BASE_ID        = 0x5,  /* resource id of AHCI base addr <BAR 5> */
			AHCI_INTR_OFF       = 0x3c, /* offset of interrupt information in config space */
			AHCI_PORT_BASE      = 0x100,
		};

		Generic_ctrl             *_ctrl;      /* generic host control */
		Ahci_port                *_port;      /* port base of device */
		Irq_connection           *_irq;       /* device IRQ */
		size_t                    _block_cnt; /* number of blocks on device */
		Command_list             *_cmd_list;  /* pointer to command list */
		Command_table            *_cmd_table; /* pointer to command table */
		Ram_dataspace_capability  _ds;        /* backing-store of internal data structures */
		Io_mem_session_capability _io_cap;    /* I/O mem cap */

		::Pci::Connection        &_pci;
		::Pci::Device_client     *_pci_device;
		::Pci::Device_capability  _pci_device_cap;

		/**
		 * Return next PCI device
		 */
		static Pci::Device_capability _scan_pci(Pci::Connection &pci, Pci::Device_capability prev_device_cap)
		{
			Pci::Device_capability device_cap;
			device_cap = pci.next_device(prev_device_cap,
			                             CLASS_MASS_STORAGE | SUBCLASS_AHCI,
			                             CLASS_MASK);

			if (prev_device_cap.valid())
				pci.release_device(prev_device_cap);

			return device_cap;
		}

		/**
		 * Find first non-ATAPI device that is ready
		 */
		bool _scan_ports()
		{
			uint32_t port_cnt = _ctrl->port_count();

			Ahci_port *port = (Ahci_port *)((char *)_ctrl + AHCI_PORT_BASE);
			for (uint32_t i = 0;
			     i <= port_cnt;
			     i++, port = (Ahci_port *)((char *)port + Ahci_port::size())) {

				bool is_atapi = port->cmd() & (1 << 24); /* check bit 24 */
				PINF("Port %u: ATAPI %s", i, is_atapi ? "yes" : "no");

				if (is_atapi)
					continue;

				/* port status */
				if (port->status_active()) {
					PINF("Port %u: Detected interface is active", i);
					_port = port;
					return true;
				}
			}

			return false;
		}

		/**
		 * Initialize port
		 */
		void _init(::Pci::Device_client *pci_device)
		{
			uint32_t version = _ctrl->version();
			PINF("AHCI Version: %x.%04x", (version >> 16), version && 0xffff);

			/* HBA capabilities at offset 0 */
			uint32_t caps = _ctrl->hba_cap();
			PINF("CAPs:");
			PINF("\tPort count: %u", _ctrl->port_count());
			PINF("\tCommand slots: %u", _ctrl->cmd_slots());
			PINF("\tAHCI only: %s", (caps & 0x20000) ? "yes" : "no");
			PINF("\tNative command queuing: %s", (caps & 0x40000000) ? "yes" : "no");
			PINF("\t64 Bit: %s", (caps & 0x80000000) ? "yes" : "no");

			/* setup up AHCI data structures */
			_setup_memory(pci_device);

			/* check and possibly enable AHCI mode */
			_ctrl->global_enable_ahci();

			/* enable global interrupts */
			_ctrl->global_interrupt_enable();

			/* disable power mgmt. */
			_port->disable_pm();

			/* startup device */
			_port->get_ready();

			/* reset port */
			_port->reset();

			/* clear error register */
			_port->err(_port->err());

			/* port interrupt enable */
			_port->interrupt_enable();

			/* ack all possibly pending interrupts */
			_port->interrupt_ack();
			_ctrl->hba_interrupt_ack();

			/* retrieve block count */
			_identify_device(pci_device);
		}

		void _setup_memory(::Pci::Device_client *pci_device)
		{
			_ds = alloc_dma_buffer(0x1000);
			addr_t   phys = Dataspace_client(_ds).phys_addr();
			uint8_t *virt = (uint8_t *)env()->rm_session()->attach(_ds);

			/* setup command list (size 1k naturally aligned) */
			_port->cmd_list_base(phys);
			_cmd_list = (struct Command_list *)(virt);

			/* for now we transfer one PRD with a FIS size of 5 byte */
			_cmd_list->prdtl = 1;
			_cmd_list->cfl   = 5;
			virt += 1024; phys += 1024;

			/* setup received FIS base (256 byte naturally aligned) */
			_port->fis_base(phys);
			virt += 256; phys += 256;

			/* setup command table (128 byte aligned (cache line size)) */
			_cmd_list->cmd_table_base_l = phys;
			_cmd_list->cmd_table_base_u = 0;
			_cmd_table = (struct Command_table *)(virt);
		}

		/**
		 * Execute ATA 'IDENTIFY DEVICE' command
		 */
		void _identify_device(Pci::Device_client *pci_device)
		{
			Ram_dataspace_capability ds = alloc_dma_buffer(0x1000);
			uint16_t *dev_info = (uint16_t *)env()->rm_session()->attach(ds);

			enum { IDENTIFY_DEVICE = 0xec };
			try {
				addr_t phys = Dataspace_client(ds).phys_addr();
				_cmd_table->setup_command(IDENTIFY_DEVICE, 0, 0, phys);
				_execute_command();

				/* XXX: just read 32 bit for now */
				_block_cnt = *((size_t *)&dev_info[100]);

			} catch (Block::Driver::Io_error) {
				PERR("I/O Error: Could not identify device");
			}

			if (verbose)
				PDBG("Max LBA48 block: %zu", _block_cnt);

			env()->rm_session()->detach(dev_info);
			env()->ram_session()->free(ds);
		}

		/**
		 * Execute a prepared command
		 */
		void _execute_command()
		{
			/* reset byte count */
			_cmd_list->prdbc = 0;

			/* start HBA command processing */
			_port->hba_enable();

			if (verbose)
				PDBG("Int status: global: %x port: %x error: %x",
				     _ctrl->hba_intr_status(), _port->intr_status(), _port->err());

			/* write CI (command issue) slot 0 */
			_port->cmd_issue(1);

			/* wait for interrupt */
			_irq->wait_for_irq();

			if (verbose)
				PDBG("Int status (IRQ): global: %x port: %x error: %x",
				     _ctrl->hba_intr_status(), _port->intr_status(), _port->err());

			/* acknowledge interrupt */
			uint32_t status = _port->interrupt_ack();

			/* check for error */
			enum {
				INT_SETUP_FIS_DMA     = 0x4,
				INT_SETUP_FIS_PIO     = 0x2,
				INT_HOST_REGISTER_FIS = 0x1,
				INT_OK = INT_SETUP_FIS_DMA | INT_SETUP_FIS_PIO | INT_HOST_REGISTER_FIS
			};

			if (!(status & INT_OK)) {
				PERR("Error during SATA request (irq state %x)", status);
				throw Block::Driver::Io_error();
			}

			/* acknowledge global port interrupt */
			_ctrl->hba_interrupt_ack();

			/* disable hba */
			_port->hba_disable();
		}

		static void _disable_msi(::Pci::Device_client *pci)
		{
			enum { PM_CAP_OFF = 0x34, MSI_CAP = 0x5, MSI_ENABLED = 0x1 };
			uint8_t cap = pci->config_read(PM_CAP_OFF, ::Pci::Device::ACCESS_8BIT);

			/* iterate through cap pointers */
			for (uint16_t val = 0; cap; cap = val >> 8) {
				val = pci->config_read(cap, ::Pci::Device::ACCESS_16BIT);

				if ((val & 0xff) != MSI_CAP)
					continue;
				uint16_t msi = pci->config_read(cap + 2, ::Pci::Device::ACCESS_16BIT);

				if (msi & MSI_ENABLED) {
					pci->config_write(cap + 2, msi ^ MSI_CAP, ::Pci::Device::ACCESS_8BIT);
					PINF("Disabled MSIs %x", msi);
				}
			}
		}

	public:

		Ahci_device(addr_t base, Io_mem_session_capability io_cap,
		            Pci::Connection &pci)
		:
			_ctrl((Generic_ctrl *)base), _port(0), _irq(0), _cmd_list(0),
			_cmd_table(0), _io_cap(io_cap), _pci(pci), _pci_device(0) {}

		~Ahci_device()
		{
			/* delete internal data structures */
			if (_ds.valid()) {
				env()->rm_session()->detach((void*)_cmd_list);
				env()->ram_session()->free(_ds);
			}

			/* close I/O mem session */
			env()->rm_session()->detach((void *)_ctrl);
			env()->parent()->close(_io_cap);

			/* XXX release _pci_device */

			/* close IRQ session */
			destroy(env()->heap(), _irq);
		}

		/**
		 * Probe PCI-bus for AHCI and ATA-devices
		 */
		static Ahci_device *probe(Pci::Connection &pci)
		{
			Pci::Device_capability device_cap;
			Ahci_device *device;

			while ((device_cap = _scan_pci(pci, device_cap)).valid()) {

				::Pci::Device_client * pci_device =
					new(env()->heap()) ::Pci::Device_client(device_cap);

				PINF("Found AHCI HBA (Vendor ID: %04x Device ID: %04x Class:"
				     " %08x)\n", pci_device->vendor_id(),
				     pci_device->device_id(), pci_device->class_code());

				/* read and map base address of AHCI controller */
				Pci::Device::Resource resource = pci_device->resource(AHCI_BASE_ID);

				Io_mem_connection io(resource.base(), resource.size());
				addr_t addr = (addr_t)env()->rm_session()->attach(io.dataspace());

				/* add possible resource offset */
				addr += resource.base() & 0xfff;

				if (verbose)
					PDBG("resource base: %x virt: %lx", resource.base(), addr);

				/* create and test device */
				device = new(env()->heap()) Ahci_device(addr, io.cap(), pci);
				if (device->_scan_ports()) {

					io.on_destruction(Io_mem_connection::KEEP_OPEN);

					/* read IRQ information */
					unsigned long intr = pci_device->config_read(AHCI_INTR_OFF,
					                                             ::Pci::Device::ACCESS_32BIT);

					if (verbose) {
						PDBG("Interrupt pin: %lu line: %lu", (intr >> 8) & 0xff, intr & 0xff);

						unsigned char bus, dev, func;
						pci_device->bus_address(&bus, &dev, &func);
						PDBG("Bus address: %x:%02x.%u (0x%x)", bus, dev, func, (bus << 8) | ((dev & 0x1f) << 3) | (func & 0x7));
					}

					/* disable message signaled interrupts */
					_disable_msi(pci_device);

					device->_irq = new(env()->heap()) Irq_connection(intr & 0xff);
					/* remember pci_device to be able to allocate ram memory which is dma able */
					device->_pci_device_cap = device_cap;
					device->_pci_device = pci_device;
					/* trigger assignment of pci device to the ahci driver */
					pci.config_extended(device_cap);

					/* get device ready */
					device->_init(pci_device);

					return device;
				}

				destroy(env()->heap(), pci_device);
				env()->rm_session()->detach(addr);
				destroy(env()->heap(), device);
			}
			return 0;
		}

		static size_t block_size()  { return 512; }
		size_t        block_count() { return _block_cnt; }

		/**
		 * Issue ATA 'READ_DMA_EXT' command
		 */
		void read(size_t block_number, size_t  block_count, addr_t phys)
		{
			_cmd_list->w = 0;

			enum { READ_DMA_EXT = 0x25 };
			_cmd_table->setup_command(READ_DMA_EXT, block_number, block_count, phys);
			_execute_command();
		}

		/**
		 * Issue ATA 'WRITE_DMA_EXT' command
		 */
		void write(size_t block_number, size_t  block_count, addr_t phys)
		{
			_cmd_list->w = 1;

			enum { WRITE_DMA_EXT = 0x35 };
			_cmd_table->setup_command(WRITE_DMA_EXT, block_number, block_count, phys);
			_execute_command();
		}

		Ram_dataspace_capability alloc_dma_buffer(size_t size) {
			return _pci.alloc_dma_buffer(_pci_device_cap, size); }
};


/**
 * Implementation of block driver interface
 */
class Ahci_driver : public Block::Driver
{
	private:

		Pci::Connection _pci;
		Ahci_device *_device;

		void _sanity_check(size_t block_number, size_t count)
		{
			if (!_device || (block_number + count > block_count()))
				throw Io_error();
		}

	public:

		Ahci_driver() : _device(Ahci_device::probe(_pci)) { }

		~Ahci_driver()
		{
			if (_device)
				destroy(env()->heap(), _device);
		}

		size_t block_size()  { return Ahci_device::block_size(); }
		size_t block_count() { return _device ? _device->block_count() : 0; }
		bool   dma_enabled() { return true; }

		void read_dma(size_t block_number,
		              size_t block_count,
		              addr_t phys)
		{
			_sanity_check(block_number, block_count);
			_device->read(block_number, block_count, phys);
		}

		void write_dma(size_t  block_number,
		               size_t  block_count,
		               addr_t  phys)
		{
			_sanity_check(block_number, block_count);
			_device->write(block_number, block_count, phys);
		}

		void read(size_t, size_t, char *)
		{
			PERR("%s should not be called", __PRETTY_FUNCTION__);
			throw Io_error();
		}

		void write(size_t, size_t, char const *)
		{
			PERR("%s should not be called", __PRETTY_FUNCTION__);
			throw Io_error();
		}

		Ram_dataspace_capability alloc_dma_buffer(size_t size) {
			return _device->alloc_dma_buffer(size); }
};


int main()
{
	printf("--- AHCI driver started ---\n");

	struct Ahci_driver_factory : Block::Driver_factory
	{
		Block::Driver *create() {
			return new(env()->heap()) Ahci_driver(); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(env()->heap(), static_cast<Ahci_driver *>(driver)); }
	} driver_factory;

	enum { STACK_SIZE = 8129 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "block_ep");

	static Block::Root block_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&block_root));

	sleep_forever();
	return 0;
}
