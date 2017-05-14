/**
 * \brief  Generic AHCI controller definitions
 * \author Sebasitan Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AHCI_H_
#define _INCLUDE__AHCI_H_

#include <block/component.h>
#include <os/attached_mmio.h>
#include <os/reporter.h>
#include <util/retry.h>
#include <util/reconstructible.h>

static bool constexpr verbose = false;

namespace Platform {
	struct Hba;
	Hba &init(Genode::Env &env, Genode::Mmio::Delayer &delayer);
};


struct Ahci_root
{
	virtual Genode::Entrypoint &entrypoint() = 0;
	virtual void announce()                  = 0;
};


namespace Ahci_driver {

	void init(Genode::Env &env, Genode::Allocator &alloc, Ahci_root &ep,
	          bool support_atapi, Genode::Signal_context_capability device_identified);

	bool avail(long device_num);
	long device_number(char const *model_num, char const *serial_num);

	Block::Driver *claim_port(long device_num);
	void           free_port(long device_num);
	void           report_ports(Genode::Reporter &reporter);

	struct Missing_controller { };
}


struct Platform::Hba
{
	/**
	 * Return base address and size of HBA device registers
	 */
	virtual Genode::addr_t base() const = 0;
	virtual Genode::size_t size() const = 0;

	/**
	 * Register interrupt signal context
	 */
	virtual void sigh_irq(Genode::Signal_context_capability sigh) = 0;
	virtual void ack_irq() = 0;

	/**
	 * DMA
	 */
	virtual Genode::Ram_dataspace_capability alloc_dma_buffer(Genode::size_t size) = 0;
	virtual void free_dma_buffer(Genode::Ram_dataspace_capability ds) = 0;
};


/**
 * HBA definitions
 */
struct Hba : Genode::Attached_mmio
{
	Mmio::Delayer &_delayer;

	Hba(Genode::Env &env, Platform::Hba &hba, Mmio::Delayer &delayer)
	: Attached_mmio(env, hba.base(), hba.size()), _delayer(delayer) { }

	/**
	 * Host capabilites
	 */
	struct Cap : Register<0x0, 32>
	{
		struct Np   : Bitfield<0, 4> { };  /* number of ports */
		struct Ncs  : Bitfield<8, 5> { };  /* number of command slots */
		struct Iss  : Bitfield<20, 4> { }; /* interface speed support */
		struct Sncq : Bitfield<30, 1> { }; /* supports native command queuing */
		struct Sa64 : Bitfield<31, 1> { }; /* supports 64 bit addressing */
	};

	unsigned port_count()    { return read<Cap::Np>() + 1; }
	unsigned command_slots() { return read<Cap::Ncs>() + 1; }
	bool     ncq()           { return !!read<Cap::Sncq>(); }
	bool     supports_64bit(){ return !!read<Cap::Sa64>(); }

	/**
	 * Generic host control
	 */
	struct Ghc : Register<0x4, 32>
	{
		struct Hr : Bitfield<0, 1> { };  /* hard reset */
		struct Ie : Bitfield<1, 1> { };  /* interrupt enable */
		struct Ae : Bitfield<31, 1> { }; /* AHCI enable */
	};

	/**
	 * Interrupt status
	 */
	struct Is : Register<0x8, 32> { };

	void ack_irq() { write<Is>(read<Is>()); }

	/**
	 * Ports implemented
	 */
	struct Pi : Register<0xc, 32> { };

	/**
	 * AHCI version
	 */
	struct Version : Register<0x10, 32>
	{
		struct Minor : Bitfield<0, 16> { };
		struct Major : Bitfield<16, 16> { };
	};

	struct Cap2 : Register<0x24, 32> { };

	void init()
	{
		/* enable AHCI */
		write<Ghc::Ae>(1);

		/* enable interrupts */
		write<Ghc::Ie>(1);
	}

	Mmio::Delayer &delayer() { return _delayer; }
};


struct Device_fis : Genode::Mmio
{
	struct Status : Register<0x2, 8>
	{
		/* ATAPI bits */
		struct Device_ready : Bitfield<6, 1> { };
	};
	struct Error  : Register<0x3, 8> { };

	Device_fis(Genode::addr_t recv_base)
	: Mmio(recv_base + 0x40) { }
};


struct Command_fis : Genode::Mmio
{
	struct Type : Register<0x0, 8> { }; /* FIS type */
	struct Bits : Register<0x1, 8, 1>
	{
		struct C : Bitfield<7, 1> { }; /* update of command register */
	};
	struct Command     : Register<0x2, 8, 1> { }; /* ATA command */
	struct Features0_7 : Register<0x3, 8> { };

	/* big-endian use byte access */
	struct Lba0_7   : Register<0x4, 8> { };
	struct Lba8_15  : Register<0x5, 8> { };
	struct Lba16_23 : Register<0x6, 8> { };
	struct Lba24    : Genode::Bitset_3<Lba0_7, Lba8_15, Lba16_23> { };
	struct Device   : Register<0x7, 8>
	{
		struct Lba   : Bitfield<6, 1> { }; /* enable LBA mode */
	};

	/* big endian */
	struct Lba24_31     : Register<0x8, 8> { };
	struct Lba32_39     : Register<0x9, 8> { };
	struct Lba40_47     : Register<0xa, 8> { };
	struct Features8_15 : Register<0xb, 8> { };
	struct Features     : Genode::Bitset_2<Features0_7, Features8_15> { };
	struct Lba48        : Genode::Bitset_3<Lba24_31, Lba32_39, Lba40_47> { };
	struct Lba          : Genode::Bitset_2<Lba24, Lba48> { }; /* LBA 0 - 47 */

	/* big endian */
	struct Sector0_7  : Register<0xc, 8, 1>
	{
		struct Tag : Bitfield<3, 5> { };
	};
	struct Sector8_15 : Register<0xd, 8> { };
	struct Sector     : Genode::Bitset_2<Sector0_7, Sector8_15> { }; /* sector count */

	Command_fis(Genode::addr_t base)
	: Mmio(base)
	{
		clear();

		enum { HOST_TO_DEVICE = 0x27 };
		write<Type>(HOST_TO_DEVICE);
	}

	static constexpr Genode::size_t size() { return 0x14; }
	void clear() { Genode::memset((void *)base(), 0, size()); }


	/************************
	 ** ATA spec commands  **
	 ************************/

	void identify_device()
	{
		write<Bits::C>(1);
		write<Device::Lba>(0);
		write<Command>(0xec);
	}

	void dma_ext(bool read, Block::sector_t block_number, Genode::size_t block_count)
	{
		write<Bits::C>(1);
		write<Device::Lba>(1);
		/* read_dma_ext : write_dma_ext */
		write<Command>(read ? 0x25 : 0x35);
		write<Lba>(block_number);
		write<Sector>(block_count);
	}

	void fpdma(bool read, Block::sector_t block_number, Genode::size_t block_count,
	           unsigned slot)
	{
		write<Bits::C>(1);
		write<Device::Lba>(1);
		/* read_fpdma : write_fpdma */
		write<Command>(read ? 0x60 : 0x61);
		write<Lba>(block_number);
		write<Features>(block_count);
		write<Sector0_7::Tag>(slot);
	}

	void atapi()
	{
		write<Bits::C>(1);
		/* packet command */
		write<Command>(0xa0);
	}
};


/**
 * AHCI command list structure header
 */
struct Command_header : Genode::Mmio
{
	struct Bits : Register<0x0, 16>
	{
		struct Cfl : Bitfield<0, 5> { };  /* command FIS length */
		struct A   : Bitfield<5, 1> { };  /* ATAPI command flag */
		struct W   : Bitfield<6, 1> { };  /* write flag */
		struct P   : Bitfield<7, 1> { };  /* prefetchable flag */
		struct C   : Bitfield<10, 1> { }; /* clear busy upon R_OK */
	};

	struct Prdtl    : Register<0x2, 16> { }; /* physical region descr. length */
	struct Prdbc    : Register<0x4, 32> { }; /* PRD byte count */
	struct Ctba0    : Register<0x8, 32> { }; /* command table base addr (low) */
	struct Ctba0_u0 : Register<0xc, 32> { }; /* command table base addr (upper) */

	Command_header(Genode::addr_t base) : Mmio(base) { }

	void cmd_table_base(Genode::addr_t base_phys)
	{
		Genode::uint64_t addr = base_phys;
		write<Ctba0>(addr);
		write<Ctba0_u0>(addr >> 32);
		write<Prdtl>(1);
		write<Bits::Cfl>(Command_fis::size() / sizeof(unsigned));
	}

	void clear_byte_count()
	{
		write<Prdbc>(0);
	}

	void atapi_command()
	{
		write<Bits::A>(1);
	}

	static constexpr Genode::size_t size() { return 0x20; }
};


/**
 * ATAPI packet 12 or 16 bytes
 */
struct Atapi_command : Genode::Mmio
{
	struct Command : Register<0, 8>   { };

	/* LBA32 big endian */
	struct Lba24_31 : Register<0x2, 8> { };
	struct Lba16_23 : Register<0x3, 8> { };
	struct Lba8_15  : Register<0x4, 8> { };
	struct Lba0_7   : Register<0x5, 8> { };
	struct Lba24    : Genode::Bitset_3<Lba0_7, Lba8_15, Lba16_23> { };
	struct Lba      : Genode::Bitset_2<Lba24, Lba24_31> { };

	/* sector count big endian */
	struct Sector8_15  : Register<0x8, 8> { };
	struct Sector0_7   : Register<0x9, 8> { };
	struct Sector      : Genode::Bitset_2<Sector0_7, Sector8_15> { };


	Atapi_command(Genode::addr_t base) : Mmio(base)
	{
		Genode::memset((void *)base, 0, 16);
	}

	void read_capacity()
	{
		write<Command>(0x25);
	}

	void test_unit_ready()
	{
		write<Command>(0x0);
	}

	void read_sense()
	{
		write<Command>(0x3);
		write<Lba8_15>(18);
	}

	void read10(Block::sector_t block_number, Genode::size_t block_count)
	{
		write<Command>(0x28);
		write<Lba>(block_number);
		write<Sector>(block_count);
	}
};


/**
 * Physical region descritpor table
 */
struct Prdt : Genode::Mmio
{
	struct Dba  : Register<0x0, 32> { }; /* data base address */
	struct Dbau : Register<0x4, 32> { }; /* data base address upper 32 bits */

	struct Bits : Register<0xc, 32>
	{
		struct Dbc : Bitfield<0, 22> { }; /* data byte count */
		struct Irq : Bitfield<31,1>  { }; /* interrupt completion */
	};

	Prdt(Genode::addr_t base, Genode::addr_t phys, Genode::size_t bytes)
	: Mmio(base)
	{
		Genode::uint64_t addr = phys;
		write<Dba>(addr);
		write<Dbau>(addr >> 32);
		write<Bits::Dbc>(bytes > 0 ? bytes - 1 : 0);
	}

	static constexpr Genode::size_t size() { return 0x10; }
};


struct Command_table
{
	Command_fis   fis;
	Atapi_command atapi_cmd;

	/* in Genode we only need one PRD (for one packet) */
	Prdt            prdt;

	Command_table(Genode::addr_t base,
	              Genode::addr_t phys,
	              Genode::size_t bytes = 0)
	: fis(base), atapi_cmd(base + 0x40),
	  prdt(base + 0x80, phys, bytes)
	{ }

	static constexpr Genode::size_t size() { return 0x100; }
};


/**
 * Minimalistic AHCI port structure to merely detect device signature
 */
struct Port_base : Genode::Mmio
{
	/**
	 * Port signature
	 */
	struct Sig : Register<0x24, 32> { };

	static constexpr Genode::addr_t offset() { return 0x100; }
	static constexpr Genode::size_t size()   { return 0x80;  }

	Port_base(unsigned number, Hba &hba)
	: Mmio(hba.base() + offset() + (number * size())) { }
};


/**
 * AHCI port
 */
struct Port : Port_base
{
	struct Not_ready : Genode::Exception { };

	Genode::Region_map &rm;
	Hba                &hba;
	Platform::Hba      &platform_hba;
	unsigned            cmd_slots = hba.command_slots();

	Genode::Ram_dataspace_capability device_ds;
	Genode::Ram_dataspace_capability cmd_ds;
	Genode::Ram_dataspace_capability device_info_ds;

	Genode::addr_t cmd_list    = 0;
	Genode::addr_t fis_base    = 0;
	Genode::addr_t cmd_table   = 0;
	Genode::addr_t device_info = 0;

	enum State {
		NONE,
		STATUS,
		TEST_READY,
		IDENTIFY,
		READY,
	};

	State state = NONE;

	Port(Genode::Region_map &rm, Hba &hba, Platform::Hba &platform_hba,
	     unsigned number)
	:
		Port_base(number, hba), rm(rm), hba(hba),
		platform_hba(platform_hba)
	{
		reset();
		if (!enable())
			throw 1;
		stop();
		wait_for(hba.delayer(), Cmd::Cr::Equal(0));
	}

	virtual ~Port()
	{
		if (device_ds.valid()) {
			rm.detach((void *)cmd_list);
			platform_hba.free_dma_buffer(device_ds);
		}

		if (cmd_ds.valid()) {
			rm.detach((void *)cmd_table);
			platform_hba.free_dma_buffer(cmd_ds);
		}

		if (device_info_ds.valid()) {
			rm.detach((void*)device_info);
			platform_hba.free_dma_buffer(device_info_ds);
		}
	}

	/**
	 * Command list base (1K length naturally aligned)
	 */
	struct Clb : Register<0x0, 32> { };

	/**
	 * Command list base upper 32 bit
	 */
	struct Clbu : Register<0x4, 32> { };

	void command_list_base(Genode::addr_t phys)
	{
		Genode::uint64_t addr = phys;
		write<Clb>(addr);
		write<Clbu>(addr >> 32);
	}

	/**
	 * FIS base address (256 bytes naturally aligned)
	 */
	struct Fb : Register<0x8, 32> { };

	/**
	 * FIS base address upper 32 bit
	 */
	struct Fbu : Register<0xc, 32> { };

	void fis_rcv_base(Genode::addr_t phys)
	{
		Genode::uint64_t addr = phys;
		write<Fb>(addr);
		write<Fbu>(addr >> 32);
	}

	/**
	 * Port interrupt status
	 */
	struct Is : Register<0x10, 32, 1>
	{
		struct Dhrs : Bitfield<0, 1> { };  /* device to host register FIS */
		struct Pss  : Bitfield<1, 1> { };  /* PIO setup FIS */
		struct Dss  : Bitfield<2, 1> { };  /* DMA setup FIS */
		struct Sdbs : Bitfield<3, 1> { };  /* Set device bit  */
		struct Pcs  : Bitfield<6, 1> { };  /* port connect change status */
		struct Prcs : Bitfield<22, 1> { }; /* PhyRdy change status */
		struct Infs : Bitfield<26, 1> { }; /* interface non-fatal error */
		struct Ifs  : Bitfield<27, 1> { }; /* interface fatal error */

		/* ncq irq */
		struct Fpdma_irq   : Sdbs { };

		/* non-ncq irq */
		struct Dma_ext_irq : Bitfield<0, 3> { };
	};

	void ack_irq()
	{
		Is::access_t status = read <Is>();

		/* clear Serr.Diag.x */
		if (Is::Pcs::get(status))
			write<Serr::Diag::X>(0);

		/* clear Serr.Diag.n */
		if (Is::Prcs::get(status))
			write<Serr::Diag::N>(0);

		write<Is>(read<Is>());
	};

	/**
	 * Port interrupt enable
	 */
	struct Ie : Register<0x14, 32, 1>
	{
		struct Dhre : Bitfield<0, 1> { };  /* device to host register FIS interrupt */
		struct Pse  : Bitfield<1, 1> { };  /* PIO setup FIS interrupt */
		struct Dse  : Bitfield<2, 1> { };  /* DMA setup FIS interrupt */
		struct Sdbe : Bitfield<3, 1> { };  /* set device bits FIS interrupt (ncq) */
		struct Ufe  : Bitfield<4, 1> { };  /* unknown FIS */
		struct Dpe  : Bitfield<5, 1> { };  /* descriptor processed */
		struct Ifne : Bitfield<26, 1> { }; /* interface non-fatal error */
		struct Ife  : Bitfield<27, 1> { }; /* interface fatal error */
		struct Hbde : Bitfield<28, 1> { }; /* host bus data error */
		struct Hbfe : Bitfield<29, 1> { }; /* host bus fatal error */
		struct Tfee : Bitfield<30, 1> { }; /* task file error */
	};

	void interrupt_enable()
	{
		Ie::access_t ie = 0;
		Ie::Dhre::set(ie, 1);
		Ie::Pse::set(ie, 1);
		Ie::Dse::set(ie, 1);
		Ie::Sdbe::set(ie, 1);
		Ie::Ufe::set(ie, 1);
		Ie::Dpe::set(ie, 1);
		Ie::Ifne::set(ie, 1);
		Ie::Hbde::set(ie, 1);
		Ie::Hbfe::set(ie, 1);
		Ie::Tfee::set(ie, 1);
		write<Ie>(~0U);
	}

	/**
	 * Port command
	 */
	struct Cmd : Register<0x18, 32>
	{
		struct St    : Bitfield<0, 1> { };  /* start */
		struct Sud   : Bitfield<1, 1> { };  /* spin-up device */
		struct Pod   : Bitfield<2, 1> { };  /* power-up device */
		struct Fre   : Bitfield<4, 1> { };  /* FIS receive enable */
		struct Cr    : Bitfield<15, 1> { }; /* command list running */
		struct Atapi : Bitfield<24, 1> { };
		struct Icc   : Bitfield<28, 4> { }; /* interface communication control */
	};

	void start()
	{
		if (read<Cmd::St>())
			return;

		try {
			wait_for(hba.delayer(), Tfd::Sts_bsy::Equal(0));
		} catch (Polling_timeout) {
			Genode::error("HBA busy unable to start command processing.");
			return;
		}

		try {
			wait_for(hba.delayer(), Tfd::Sts_drq::Equal(0));
		} catch (Polling_timeout) {
			Genode::error("HBA in DRQ unable to start command processing.");
			return;
		}

		write<Cmd::St>(1);
	}

	void stop()
	{
		if (!(read<Ci>() | read<Sact>()))
			write<Cmd::St>(0);
	}

	void power_up()
	{
		Cmd::access_t cmd = read<Cmd>();
		Cmd::Sud::set(cmd, 1);
		Cmd::Pod::set(cmd, 1);
		Cmd::Fre::set(cmd, 1);
		write<Cmd>(cmd);
	}

	/**
	 * Task file data
	 */
	struct Tfd : Register<0x20, 32>
	{
		struct Sts_drq : Bitfield<3, 1> { }; /* indicates data transfer request */
		struct Sts_bsy : Bitfield<7, 1> { }; /* interface is busy */
	};

	/**
	 * Serial ATA status
	 */
	struct Ssts : Register<0x28, 32>
	{
		struct Dec : Bitfield<0, 4> /* device detection */
		{
			enum {
				NONE        = 0x0,
				ESTABLISHED = 0x3
			};
		};
		struct Ipm : Bitfield<8, 4>  /* interface power mgmt */
		{
			enum {
				ACTIVE  = 0x1,
				SUSPEND = 0x2,
			};
		};
	};

	bool enable()
	{
		Ssts::access_t status = read<Ssts>();
		if (Ssts::Dec::get(status) == Ssts::Dec::NONE)
			return false;

		/* if in power-mgmt state (partial or slumber)  */
		if (Ssts::Ipm::get(status) & Ssts::Ipm::SUSPEND) {
			/* try to wake up device */
			write<Cmd::Icc>(Ssts::Ipm::ACTIVE);

			Genode::retry<Not_ready>(
				[&] {
							if ((Ssts::Dec::get(status) != Ssts::Dec::ESTABLISHED) ||
							    !(Ssts::Ipm::get(status) &  Ssts::Ipm::ACTIVE))
								throw Not_ready();
				},
				[&] {
					hba.delayer().usleep(1000);
					status = read<Ssts>();
				}, 10);
		}

		return ((Ssts::Dec::get(status) == Ssts::Dec::ESTABLISHED) &&
		        (Ssts::Ipm::get(status) &  Ssts::Ipm::ACTIVE));
	}

	/**
	 * Serial ATA control
	 */
	struct Sctl : Register<0x2c, 32>
	{
		struct Det  : Bitfield<0, 4> { }; /* device dectection initialization */
		struct Ipmt : Bitfield<8, 4> { }; /* allowed power management transitions */
	};

	void reset()
	{
		if (read<Cmd::St>())
			Genode::warning("CMD.ST bit set during device reset --> unknown behavior");

		write<Sctl::Det>(1);
		hba.delayer().usleep(1000);
		write<Sctl::Det>(0);

		try {
			wait_for(hba.delayer(), Ssts::Dec::Equal(Ssts::Dec::ESTABLISHED));
		} catch (Polling_timeout) {
			Genode::warning("Port reset failed");
		}
	}

	/**
	 * Serial ATA error
	 */
	struct Serr : Register<0x30, 32>
	{
		struct Diag : Register<0x2, 16>
		{
			struct N : Bitfield<0,  1> { }; /* PhyRdy change */
			struct X : Bitfield<10, 1> { }; /* exchanged */
		};
	};

	void clear_serr() { write<Serr>(read<Serr>()); }

	/**
	 * Serial ATA active
	 */
	struct Sact : Register<0x34, 32> { };

	/**
	 * Command issue
	 */
	struct Ci : Register<0x38, 32> { };

	void init()
	{
		/* stop command list processing */
		stop();

		/* setup command list/table */
		setup_memory();

		/* disallow all power-management transitions */
		write<Sctl::Ipmt>(0x3);

		/* power-up device */
		power_up();

		/* reset port */
		reset();

		/* clean error register */
		clear_serr();

		/* enable required interrupts */
		interrupt_enable();

		/* acknowledge all pending interrupts */
		ack_irq();
		hba.ack_irq();
	}

	void setup_memory()
	{
		using Genode::addr_t;
		using Genode::size_t;

		device_ds  = platform_hba.alloc_dma_buffer(0x1000);

		/* command list 1K */
		addr_t phys = Genode::Dataspace_client(device_ds).phys_addr();
		cmd_list    = (addr_t)rm.attach(device_ds);
		command_list_base(phys);

		/* receive FIS base 256 byte */
		fis_base = cmd_list + 1024;
		fis_rcv_base(phys + 1024);

		/* command table */
		size_t cmd_size = Genode::align_addr(cmd_slots * Command_table::size(), 12);
		cmd_ds          = platform_hba.alloc_dma_buffer(cmd_size);
		cmd_table       = (addr_t)rm.attach(cmd_ds);
		phys            = (addr_t)Genode::Dataspace_client(cmd_ds).phys_addr();

		/* set command table addresses in command list */
		for (unsigned i = 0; i < cmd_slots; i++) {
			Command_header h(cmd_list + (i * Command_header::size()));
			h.cmd_table_base(phys + (i * Command_table::size()));
		}

		/* dataspace for device info */
		device_info_ds = platform_hba.alloc_dma_buffer(0x1000);
		device_info    = rm.attach(device_info_ds);
	}

	Genode::addr_t command_table_addr(unsigned slot)
	{
		return cmd_table + (slot * Command_table::size());
	};

	Genode::addr_t command_header_addr(unsigned slot)
	{
		return cmd_list + (slot * Command_header::size());
	}

	void execute(unsigned slot)
	{
		start();
		write<Ci>(1U << slot);
	}

	bool ready() { return state == READY; }
};


struct Port_driver : Port, Block::Driver
{
	Ahci_root &root;
	unsigned  &sem;

	Port_driver(Genode::Ram_session &ram,
	            Ahci_root           &root,
	            unsigned            &sem,
	            Genode::Region_map  &rm,
	            Hba                 &hba,
	            Platform::Hba       &platform_hba,
	            unsigned             number)
	: Port(rm, hba, platform_hba, number), Block::Driver(ram), root(root),
	  sem(sem) { sem++; }

	virtual void handle_irq() = 0;

	void state_change()
	{
		if (--sem) return;

		/* announce service */
		root.announce();
	}

	void sanity_check(Block::sector_t block_number, Genode::size_t count)
	{
		/* max. PRDT size is 4MB */
		if (count * block_size() > 4 * 1024 * 1024) {
			Genode::error("error: maximum supported packet size is 4MB");
			throw Io_error();
		}

		/* sanity check */
		if (block_number + count > block_count()) {
			Genode::error("error: requested blocks are outside of device");
			throw Io_error();
		}
	}


	/*******************
	 ** Block::Driver **
	 *******************/

	Genode::Ram_dataspace_capability
	alloc_dma_buffer(Genode::size_t size) override
	{
		return platform_hba.alloc_dma_buffer(size);
	}

	void free_dma_buffer(Genode::Ram_dataspace_capability c) override
	{
		platform_hba.free_dma_buffer(c);
	}
};

#endif /* _INCLUDE__AHCI_H_ */
