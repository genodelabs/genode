/*
 * \brief  Generic base of AHCI device
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \author Martin Stein    <Martin.Stein@genode-labs.com>
 * \date   2011-08-10
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _AHCI_DEVICE_BASE_H_
#define _AHCI_DEVICE_BASE_H_

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <block/driver.h>
#include <block/component.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <irq_session/connection.h>
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

		/* command list base */
		void cmd_list_base(addr_t cmd_base)
		{
			uint64_t addr = cmd_base;
			_set(0x0, addr);
			_set(0x4, addr >> 32);
		}

		/* receive FIS base address */
		void fis_base(addr_t fis_base)
		{
			uint64_t addr = fis_base;

			_set(0x8, addr);
			_set(0xc, addr);
		}

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

		uint64_t addr = phys_addr;

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
		uint32_t addr_l = addr;
		uint32_t addr_u = addr >> 32;
		memcpy(&fis[0x80], &addr_l, 4); /* DBA: data base address */
		memcpy(&fis[0x84], &addr_u, 4); /* DBA: data base address upper */
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
 * Generic base of AHCI device
 */
class Ahci_device_base
{
	protected:

		enum {
			AHCI_PORT_BASE = 0x100,
		};

		Generic_ctrl             *_ctrl;      /* generic host control */
		Ahci_port                *_port;      /* port base of device */
		Irq_connection           *_irq;       /* device IRQ */
		size_t                    _block_cnt; /* number of blocks on device */
		Command_list             *_cmd_list;  /* pointer to command list */
		Command_table            *_cmd_table; /* pointer to command table */
		Ram_dataspace_capability  _ds;        /* backing-store of internal data structures */
		Io_mem_session_capability _io_cap;    /* I/O mem cap */

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

		void _setup_memory()
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

			uint64_t addr = phys;
			/* setup command table (128 byte aligned (cache line size)) */
			_cmd_list->cmd_table_base_l = addr;
			_cmd_list->cmd_table_base_u = addr >> 32;
			_cmd_table = (struct Command_table *)(virt);
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

		/**
		 * Execute ATA 'IDENTIFY DEVICE' command
		 */
		void _identify_device()
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

		Ahci_device_base(addr_t base, Io_mem_session_capability io_cap)
		: _ctrl((Generic_ctrl *)base), _port(0), _irq(0), _cmd_list(0),
		  _cmd_table(0), _io_cap(io_cap) { }

	public:

		virtual ~Ahci_device_base()
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

		virtual Ram_dataspace_capability alloc_dma_buffer(size_t size) = 0;
};

#endif /* _AHCI_DEVICE_BASE_H_ */

