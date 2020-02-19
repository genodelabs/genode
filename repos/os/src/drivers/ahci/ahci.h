/**
 *
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

#ifndef _AHCI__AHCI_H_
#define _AHCI__AHCI_H_

#include <block/request_stream.h>
#include <os/attached_mmio.h>
#include <os/reporter.h>
#include <util/retry.h>
#include <util/reconstructible.h>

#include <platform.h>

static bool constexpr verbose = false;

namespace Ahci {
	struct Missing_controller : Exception { };

	class  Platform;
	struct Protocol;
	struct Port;
	struct Port_base;
	struct Hba;

	using Response       = Block::Request_stream::Response;
	using block_number_t = Block::block_number_t;
	using block_count_t  = Block::block_count_t;
}

class Ahci::Platform
{
	private :

		Data _data;

	protected:

		/**
		 * Return base address and size of HBA device registers
		 */
		addr_t _mmio_base() const;

	public:

		Platform(Env &env) : _data(env) { };

		/**
		 * Register interrupt signal context
		 */
		void sigh_irq(Signal_context_capability sigh);
		void ack_irq();

		/**
		 * DMA
		 */
		Ram_dataspace_capability alloc_dma_buffer(size_t size);
		void free_dma_buffer(Ram_dataspace_capability ds);
};

/**
 * HBA definitions
 */
struct Ahci::Hba : Ahci::Platform,
                   Mmio
{
	Mmio::Delayer &_delayer;

	Hba(Env &env, Mmio::Delayer &delayer)
	: Platform(env), Mmio(_mmio_base()), _delayer(delayer) { }

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

	void ack_irq()
	{
		write<Is>(read<Is>());
		Platform::ack_irq();
	}

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


/***********************************
 ** AHCI commands and descriptors **
 ***********************************/

namespace Ahci {

	struct Device_fis : Mmio
	{
		struct Status : Register<0x2, 8>
		{
			/* ATAPI bits */
			struct Device_ready : Bitfield<6, 1> { };
		};
		struct Error  : Register<0x3, 8> { };

		Device_fis(addr_t recv_base)
		: Mmio(recv_base + 0x40) { }
	};


	struct Command_fis : Mmio
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
		struct Lba24    : Bitset_3<Lba0_7, Lba8_15, Lba16_23> { };
		struct Device   : Register<0x7, 8>
		{
			struct Lba   : Bitfield<6, 1> { }; /* enable LBA mode */
		};

		/* big endian */
		struct Lba24_31     : Register<0x8, 8> { };
		struct Lba32_39     : Register<0x9, 8> { };
		struct Lba40_47     : Register<0xa, 8> { };
		struct Features8_15 : Register<0xb, 8> { };
		struct Features     : Bitset_2<Features0_7, Features8_15> { };
		struct Lba48        : Bitset_3<Lba24_31, Lba32_39, Lba40_47> { };
		struct Lba          : Bitset_2<Lba24, Lba48> { }; /* LBA 0 - 47 */

		/* big endian */
		struct Sector0_7  : Register<0xc, 8, 1>
		{
			struct Tag : Bitfield<3, 5> { };
		};
		struct Sector8_15 : Register<0xd, 8> { };
		struct Sector     : Bitset_2<Sector0_7, Sector8_15> { }; /* sector count */

		Command_fis(addr_t base)
		: Mmio(base)
		{
			clear();

			enum { HOST_TO_DEVICE = 0x27 };
			write<Type>(HOST_TO_DEVICE);
		}

		static constexpr size_t size() { return 0x14; }
		void clear() { memset((void *)base(), 0, size()); }


		/************************
		 ** ATA spec commands  **
		 ************************/

		void identify_device()
		{
			write<Bits::C>(1);
			write<Device::Lba>(0);
			write<Command>(0xec);
		}

		void dma_ext(bool read, block_number_t block_number, block_count_t block_count)
		{
			write<Bits::C>(1);
			write<Device::Lba>(1);
			/* read_dma_ext : write_dma_ext */
			write<Command>(read ? 0x25 : 0x35);
			write<Lba>(block_number);
			write<Sector>(block_count);
		}

		void fpdma(bool read, block_number_t block_number, block_count_t block_count,
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

		void flush_cache_ext()
		{
			write<Bits::C>(1);
			write<Device::Lba>(0);
			write<Command>(0xea);
		}

		void atapi()
		{
			write<Bits::C>(1);
			/* packet command */
			write<Command>(0xa0);
		}

		/*
		 * Sets byte count limit for PIO transfers
		 */
		void byte_count(uint16_t bytes)
		{
			write<Lba8_15>(bytes & 0xff);
			write<Lba16_23>(bytes >> 8);
		}
	};


	/**
	 * AHCI command list structure header
	 */
	struct Command_header : Mmio
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

		Command_header(addr_t base) : Mmio(base) { }

		void cmd_table_base(addr_t base_phys)
		{
			uint64_t addr = base_phys;
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

		static constexpr size_t size() { return 0x20; }
	};


	/**
	 * ATAPI packet 12 or 16 bytes
	 */
	struct Atapi_command : Mmio
	{
		struct Command : Register<0, 8>   { };

		/* LBA32 big endian */
		struct Lba24_31 : Register<0x2, 8> { };
		struct Lba16_23 : Register<0x3, 8> { };
		struct Lba8_15  : Register<0x4, 8> { };
		struct Lba0_7   : Register<0x5, 8> { };
		struct Lba24    : Bitset_3<Lba0_7, Lba8_15, Lba16_23> { };
		struct Lba      : Bitset_2<Lba24, Lba24_31> { };

		/* sector count big endian */
		struct Sector8_15  : Register<0x8, 8> { };
		struct Sector0_7   : Register<0x9, 8> { };
		struct Sector      : Bitset_2<Sector0_7, Sector8_15> { };


		Atapi_command(addr_t base) : Mmio(base)
		{
			memset((void *)base, 0, 16);
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

		void read10(block_number_t block_number, block_count_t block_count)
		{
			write<Command>(0x28);
			write<Lba>(block_number);
			write<Sector>(block_count);
		}

		void start_unit()
		{
			write<Command>(0x1b);
		}
	};


	/**
	 * Physical region descritpor table
	 */
	struct Prdt : Mmio
	{
		struct Dba  : Register<0x0, 32> { }; /* data base address */
		struct Dbau : Register<0x4, 32> { }; /* data base address upper 32 bits */

		struct Bits : Register<0xc, 32>
		{
			struct Dbc : Bitfield<0, 22> { }; /* data byte count */
			struct Irq : Bitfield<31,1>  { }; /* interrupt completion */
		};

		Prdt(addr_t base, addr_t phys, size_t bytes)
		: Mmio(base)
		{
			uint64_t addr = phys;
			write<Dba>(addr);
			write<Dbau>(addr >> 32);
			write<Bits::Dbc>(bytes > 0 ? bytes - 1 : 0);
		}

		static constexpr size_t size() { return 0x10; }
	};


	struct Command_table
	{
		Command_fis   fis;
		Atapi_command atapi_cmd;

		/* in Genode we only need one PRD (for one packet) */
		Prdt            prdt;

		Command_table(addr_t base,
		              addr_t phys,
		              size_t bytes = 0)
		: fis(base), atapi_cmd(base + 0x40),
		  prdt(base + 0x80, phys, bytes)
		{ }

		static constexpr size_t size() { return 0x100; }
	};
} /* namespace Ahci */

/**
 * Minimalistic AHCI port structure to merely detect device signature
 */
struct Ahci::Port_base : Mmio
{
	/* device signature */
	enum Signature {
		ATA_SIG        = 0x101,
		ATAPI_SIG      = 0xeb140101,
		ATAPI_SIG_QEMU = 0xeb140000, /* will be fixed in Qemu */
	};

	unsigned  index { };
	Hba      &hba;

	/**
	 * Port signature
	 */
	struct Sig : Register<0x24, 32> { };

	static constexpr addr_t offset() { return 0x100; }
	static constexpr size_t size()   { return 0x80;  }

	Port_base(unsigned index, Hba &hba)
	: Mmio(hba.base() + offset() + (index * size())),
	  index(index), hba(hba) { }

	bool implemented() const
	{
		return hba.read<Hba::Pi>() & (1u << index);
	}

	bool ata() const { return read<Sig>() == ATA_SIG; }

	bool atapi() const
	{
		unsigned sig = read<Sig>();
		return sig == ATAPI_SIG || sig == ATAPI_SIG_QEMU;
	}
};


struct Ahci::Protocol : Interface
{
	virtual unsigned init(Port &port) = 0;
	virtual Block::Session::Info info() const = 0;
	virtual void handle_irq(Port &port) = 0;

	virtual Response submit(Port &port, Block::Request const request) = 0;
	virtual Block::Request completed(Port &port) = 0;

	virtual void writeable(bool rw) = 0;
};

/**
 * AHCI port
 */
struct Ahci::Port : private Port_base
{
	using Port_base::write;
	using Port_base::read;
	using Port_base::wait_for_any;
	using Port_base::wait_for;
	using Port_base::Register_set::Polling_timeout;
	using Port_base::index;
	using Port_base::hba;

	struct Not_ready : Exception { };

	Protocol    &protocol;
	Region_map  &rm;
	unsigned     cmd_slots = hba.command_slots();

	Ram_dataspace_capability device_ds      { };
	Ram_dataspace_capability cmd_ds         { };
	Ram_dataspace_capability device_info_ds { };

	addr_t cmd_list      = 0;
	addr_t fis_base      = 0;
	addr_t cmd_table     = 0;
	addr_t device_info   = 0;
	addr_t dma_base      = 0; /* physical address of DMA memory */

	Port(Protocol &protocol, Region_map &rm, Hba &hba,
	     unsigned index)
	:
		Port_base(index, hba),
		protocol(protocol), rm(rm)
	{
		reset();
		if (!enable())
			throw 1;

		stop();

		wait_for(hba.delayer(), Cmd::Cr::Equal(0));

		init();

		/*
		 * Init protocol and determine actual number of command slots of device
		 */
		try {
			unsigned device_slots = protocol.init(*this);
			cmd_slots = min(device_slots, cmd_slots);
		} catch (Polling_timeout) {
			/* ack any pending IRQ from failed device initialization */
			ack_irq();
			throw;
		}
	}

	virtual ~Port()
	{
		if (device_ds.valid()) {
			rm.detach((void *)cmd_list);
			hba.free_dma_buffer(device_ds);
		}

		if (cmd_ds.valid()) {
			rm.detach((void *)cmd_table);
			hba.free_dma_buffer(cmd_ds);
		}

		if (device_info_ds.valid()) {
			rm.detach((void*)device_info);
			hba.free_dma_buffer(device_info_ds);
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

	void command_list_base(addr_t phys)
	{
		uint64_t addr = phys;
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

	void fis_rcv_base(addr_t phys)
	{
		uint64_t addr = phys;
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
		struct Fr    : Bitfield<14, 1> { }; /* FIS receive running */
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
			error("HBA busy unable to start command processing.");
			return;
		}

		try {
			wait_for(hba.delayer(), Tfd::Sts_drq::Equal(0));
		} catch (Polling_timeout) {
			error("HBA in DRQ unable to start command processing.");
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

			retry<Not_ready>(
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
			warning("CMD.ST bit set during device reset --> unknown behavior");

		write<Sctl::Det>(1);
		hba.delayer().usleep(1000);
		write<Sctl::Det>(0);

		try {
			wait_for(hba.delayer(), Ssts::Dec::Equal(Ssts::Dec::ESTABLISHED));
		} catch (Polling_timeout) {
			warning("Port reset failed");
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
	 * Serial ATA active (strict write)
	 */
	struct Sact : Register<0x34, 32, true> { };

	/**
	 * Command issue (strict write)
	 */
	struct Ci : Register<0x38, 32, true> { };

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
		device_ds  = hba.alloc_dma_buffer(0x1000);

		/* command list 1K */
		addr_t phys = Dataspace_client(device_ds).phys_addr();
		cmd_list    = (addr_t)rm.attach(device_ds);
		command_list_base(phys);

		/* receive FIS base 256 byte */
		fis_base = cmd_list + 1024;

		/*
		 *  Set fis receive base, clear Fre (FIS receive) before and wait for FR
		 *  (FIS receive running) to clear
		 */
		write<Cmd::Fre>(0);
		wait_for(hba.delayer(), Cmd::Fr::Equal(0));
		fis_rcv_base(phys + 1024);

		/* command table */
		size_t cmd_size = align_addr(cmd_slots * Command_table::size(), 12);
		cmd_ds          = hba.alloc_dma_buffer(cmd_size);
		cmd_table       = (addr_t)rm.attach(cmd_ds);
		phys            = (addr_t)Dataspace_client(cmd_ds).phys_addr();

		/* set command table addresses in command list */
		for (unsigned i = 0; i < cmd_slots; i++) {
			Command_header h(cmd_list + (i * Command_header::size()));
			h.cmd_table_base(phys + (i * Command_table::size()));
		}

		/* dataspace for device info */
		device_info_ds = hba.alloc_dma_buffer(0x1000);
		device_info    = rm.attach(device_info_ds);
	}

	addr_t command_table_addr(unsigned slot)
	{
		return cmd_table + (slot * Command_table::size());
	};

	addr_t command_header_addr(unsigned slot)
	{
		return cmd_list + (slot * Command_header::size());
	}

	void execute(unsigned slot)
	{
		start();
		write<Ci>(1U << slot);
	}

	bool sanity_check(Block::Request const &request)
	{
		Block::Session::Info const i = info();

		/* max. PRDT size is 4MB */
		if (request.operation.count * i.block_size > 4 * 1024 * 1024) {
			error("error: maximum supported packet size is 4MB");
			return false;
		}

		/* sanity check */
		if (request.operation.count + request.operation.block_number > i.block_count) {
			error("error: requested blocks are outside of device");
			return false;
		}

		return true;
	}

	Ram_dataspace_capability alloc_buffer(size_t size)
	{
		if (dma_base) return Ram_dataspace_capability();

		Ram_dataspace_capability dma = hba.alloc_dma_buffer(size);
		dma_base = Dataspace_client(dma).phys_addr();
		return dma;
	}

	void free_buffer(Ram_dataspace_capability ds)
	{
		dma_base = 0;
		hba.free_dma_buffer(ds);
	}

	/**********************
	 ** Protocol wrapper **
	 **********************/


	Block::Session::Info info() const { return protocol.info(); }
	void handle_irq() { protocol.handle_irq(*this); }

	Response submit(Block::Request const request) {
		return protocol.submit(*this, request); }

	template <typename FN>
	void for_one_completed_request(FN const &fn)
	{
		Block::Request request = protocol.completed(*this);
		if (!request.operation.valid()) return;

		request.success = true;
		fn(request);
	}

	void writeable(bool rw) { protocol.writeable(rw); }
};

#endif /* _AHCI__AHCI_H_ */
