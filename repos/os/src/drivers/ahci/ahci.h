/**
 *
 * \brief  Generic AHCI controller definitions
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AHCI__AHCI_H_
#define _AHCI__AHCI_H_

#include <block/request_stream.h>
#include <platform_session/device.h>
#include <platform_session/dma_buffer.h>
#include <os/reporter.h>
#include <util/retry.h>
#include <util/reconstructible.h>

static bool constexpr verbose = false;

namespace Ahci {
	using namespace Genode;

	struct Missing_controller : Exception { };

	struct Protocol;
	struct Port;
	struct Port_base;
	struct Port_mmio;
	struct Hba;
	struct Hba_mmio;
	struct Resources;

	using Response       = Block::Request_stream::Response;
	using block_number_t = Block::block_number_t;
	using block_count_t  = Block::block_count_t;
}


/**
 * HBA mmio definitions
 */
struct Ahci::Hba_mmio : Platform::Device::Mmio<0x28>
{
	using Platform::Device::Mmio<SIZE>::base;
	using Index = Platform::Device::Mmio<SIZE>::Index;

	Hba_mmio(Platform::Device &device, Platform::Device::Mmio<0x28>::Index const &index)
	: Platform::Device::Mmio<0x28>(device, index)
	{ }

	/**
	 * Host capabilities
	 */
	struct Cap : Register<0x0, 32>
	{
		struct Np   : Bitfield< 0, 4> { }; /* number of ports */
		struct Ncs  : Bitfield< 8, 5> { }; /* number of command slots */
		struct Iss  : Bitfield<20, 4> { }; /* interface speed support */
		struct Sncq : Bitfield<30, 1> { }; /* supports native command queuing */
		struct Sa64 : Bitfield<31, 1> { }; /* supports 64 bit addressing */
	};

	/**
	 * Generic host control
	 */
	struct Ghc : Register<0x4, 32>
	{
		struct Hr : Bitfield< 0, 1> { }; /* hard reset */
		struct Ie : Bitfield< 1, 1> { }; /* interrupt enable */
		struct Ae : Bitfield<31, 1> { }; /* AHCI enable */
	};

	/**
	 * Interrupt status
	 */
	struct Is : Register<0x8, 32> { };

	/**
	 * Ports implemented
	 *
	 * Each bit set here corresponds to a port that can be accessed by software
	 */
	struct Pi : Register<0xc, 32> { };

	/**
	 * AHCI version
	 */
	struct Version : Register<0x10, 32>
	{
		struct Minor : Bitfield< 0, 16> { };
		struct Major : Bitfield<16, 16> { };
	};

	struct Cap2 : Register<0x24, 32> { };
};


/**
 * HBA definitions
 */
struct Ahci::Hba
{
	Resources &_resource;

	void with_mmio (auto const &, auto const &);
	void with_mmio (auto const &, auto const &) const;

	Hba(Resources &resource) : _resource(resource)
	{
		reinit();
	}

	void reinit()
	{
		with_mmio([&](Hba_mmio &mmio) {

			log("version: "
			    "major=", Hex(mmio.read<Hba_mmio::Version::Major>()), " "
			    "minor=", Hex(mmio.read<Hba_mmio::Version::Minor>()));
			log("command slots: ", command_slots());
			log("native command queuing: ", ncq() ? "yes" : "no");
			log("64-bit support: ", supports_64bit() ? "yes" : "no");

			/* enable AHCI */
			mmio.write<Hba_mmio::Ghc::Ae>(1);

			/* enable interrupts */
			mmio.write<Hba_mmio::Ghc::Ie>(1);
		}, [&]() { error("Hba init failed"); });
	}

	unsigned port_count() const
	{
		unsigned result = 0;

		with_mmio([&](Hba_mmio const &mmio) {
			result = mmio.read<Hba_mmio::Cap::Np>() + 1;
		}, [&] () { /* no port count in case hardware is not available */ });

		return result;
	}

	unsigned command_slots() const
	{
		unsigned result = 0;

		with_mmio([&](Hba_mmio const &mmio) {
			result = mmio.read<Hba_mmio::Cap::Ncs>() + 1;
		}, [&] () { /* no mmio, no commands */ });

		return result;
	}

	bool ncq() const
	{
		bool result = false;

		with_mmio([&](Hba_mmio const &mmio) {
			result = !!mmio.read<Hba_mmio::Cap::Sncq>();
		}, [&] () { /* no mmio, no ncq */ });

		return result;
	}

	bool supports_64bit() const
	{
		bool result = false;

		with_mmio([&](Hba_mmio const &mmio) {
			result = !!mmio.read<Hba_mmio::Cap::Sa64>();
		}, [&] () { /* no mmio, no 64bit */ });

		return result;
	}

	bool port_implemented(unsigned port) const
	{
		bool result = false;

		with_mmio([&](Hba_mmio const &mmio) {
			result = mmio.read<Hba_mmio::Pi>() & (1u << port);
		}, [&] () { /* no mmio, no port */ });

		return result;
	}

	/* for diagnostics */
	unsigned pi_value() const
	{
		unsigned value = 0;

		with_mmio([&](Hba_mmio const &mmio) {
			 value = mmio.read<Hba_mmio::Pi>();
		}, [&] () { /* no mmio, no diagnostic */ });

		return value;
	}

	unsigned cap_np_value() const
	{
		unsigned value = 0;

		with_mmio([&](Hba_mmio const &mmio) {
			value = mmio.read<Hba_mmio::Cap::Np>();
		}, [&] () { /* no mmio, no diagnostic */ });

		return value;
	}

	void ack_irq();

	void handle_irq(auto const &fn, auto const &fn_error)
	{
		with_mmio([&](Hba_mmio &mmio) {
			unsigned port_list = mmio.read<Hba_mmio::Is>();
			while (port_list) {
				unsigned port = log2(port_list);
				port_list    &= ~(1U << port);
				fn(port);
			}

			/* clear status register */
			ack_irq();
		}, fn_error);
	}
};


struct Ahci::Resources
{
	private:

		using Device = Platform::Device;

		Platform::Connection              _platform;
		Signal_context_capability const   _irq_cap;
		Reconstructible<Device>           _device { _platform };
		Reconstructible<Device::Irq>      _irq    { *_device };
		Reconstructible<Hba_mmio>         _mmio   { *_device, _mmio_index(_platform) };
		Hba                               _hba    { *this };


		/*
		 * mmio region of AHCI controller is always in BAR 5
		 */
		class No_bar : Genode::Exception { };


		Hba_mmio::Index _mmio_index(Platform::Connection &platform)
		{
			unsigned index = 0;
			unsigned bar5  = ~0u;

			platform.update();

			platform.with_xml([&] (Xml_node const & xml) {
				xml.with_optional_sub_node("device", [&] (Xml_node const &xml) {
					xml.for_each_sub_node("io_mem", [&] (Xml_node const &node) {
						unsigned bar = node.attribute_value("pci_bar", ~0u);
						if (bar == 5) bar5 = index;
						index++;
					});
				});
			});

			if (bar5 == ~0u) {
				error("MMIO region of HBA (BAR 5) not found. Try adding\n"
				      "<policy info=\"yes\" ...>\n"
				      "to platform driver configuration.");
				throw No_bar();
			}

			return { bar5 };
		}

		void reinit()
		{
			if (_irq.constructed())
				_irq->sigh(_irq_cap);
		}

	public:

		Resources(Env &env, Signal_context_capability const &irq_cap)
		:
			_platform(env), _irq_cap(irq_cap)
		{
			reinit();
		}

		void release_device()
		{
			if (_device.constructed())
				log("release AHCI device for suspend");

			_mmio  .destruct();
			_irq   .destruct();
			_device.destruct();
		}

		void acquire_device()
		{
			_device.construct(_platform);
			_irq   .construct(*_device);
			_mmio  .construct(*_device, _mmio_index(_platform));

			_hba.reinit();

			reinit();
		}

		void with_mmio_irq(auto const &fn, auto const &fn_error)
		{
			if (_mmio.constructed() && _irq.constructed())
				fn(*_mmio, *_irq);
			else
				fn_error();
		}

		void with_mmio(auto const &fn, auto const &fn_error)
		{
			if (_mmio.constructed())
				fn(*_mmio);
			else
				fn_error();
		}

		void with_mmio(auto const &fn, auto const &fn_error) const
		{
			if (_mmio.constructed())
				fn(*_mmio);
			else
				fn_error();
		}

		void with_hba(auto const &fn)
		{
			fn(_hba);
		}

		void with_platform(auto const &fn) { fn(_platform); }
};


/***********************************
 ** AHCI commands and descriptors **
 ***********************************/

namespace Ahci {

	struct Device_fis : Mmio<0x4>
	{
		struct Status : Register<0x2, 8>
		{
			/* ATAPI bits */
			struct Device_ready : Bitfield<6, 1> { };
		};
		struct Error  : Register<0x3, 8> { };

		Device_fis(Byte_range_ptr const &recv_range)
		: Mmio({recv_range.start + 0x40, recv_range.num_bytes - 0x40}) { }
	};


	struct Command_fis : Mmio<0xe>
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

		Command_fis(Byte_range_ptr const &range)
		: Mmio(range)
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
			write<Sector>((uint16_t)(block_count));
		}

		void fpdma(bool read, block_number_t block_number, block_count_t block_count,
		           unsigned slot)
		{
			write<Bits::C>(1);
			write<Device::Lba>(1);
			/* read_fpdma : write_fpdma */
			write<Command>(read ? 0x60 : 0x61);
			write<Lba>(block_number);
			write<Features>((uint16_t)block_count);
			write<Sector0_7::Tag>((uint8_t)slot);
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
			write<Lba8_15> ((uint8_t)(bytes        & 0xff));
			write<Lba16_23>((uint8_t)((bytes >> 8) & 0xff));
		}
	};


	/**
	 * AHCI command list structure header
	 */
	struct Command_header : Mmio<0x10>
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

		using Mmio::Mmio;

		void cmd_table_base(addr_t base_phys)
		{
			uint64_t addr = base_phys;
			write<Ctba0>((uint32_t)addr);
			write<Ctba0_u0>((uint32_t)(addr >> 32));
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
	struct Atapi_command : Mmio<0xa>
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


		Atapi_command(Byte_range_ptr const &range) : Mmio(range)
		{
			memset((void *)base(), 0, 16);
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
			write<Lba>((uint32_t)block_number);
			write<Sector>((uint16_t)block_count);
		}

		void start_unit()
		{
			write<Command>(0x1b);
		}
	};


	/**
	 * Physical region descritpor table
	 */
	struct Prdt : Mmio<0x10>
	{
		struct Dba  : Register<0x0, 32> { }; /* data base address */
		struct Dbau : Register<0x4, 32> { }; /* data base address upper 32 bits */

		struct Bits : Register<0xc, 32>
		{
			struct Dbc : Bitfield<0, 22> { }; /* data byte count */
			struct Irq : Bitfield<31,1>  { }; /* interrupt completion */
		};

		Prdt(Byte_range_ptr const &range, addr_t phys, size_t bytes)
		: Mmio(range)
		{
			uint64_t const addr = phys;

			write<Dba>((uint32_t)addr);
			write<Dbau>((uint32_t)(addr >> 32));
			write<Bits::Dbc>((uint32_t)(bytes > 0 ? bytes - 1 : 0));
		}

		static constexpr size_t size() { return 0x10; }
	};


	struct Command_table
	{
		Command_fis   fis;
		Atapi_command atapi_cmd;
		/* in Genode we only need one PRD (for one packet) */
		Prdt          prdt;

		enum { ATAPI_CMD_OFF = 0x40 };
		enum { PRDT_OFF = 0x80 };

		Command_table(Byte_range_ptr const &range,
		              addr_t phys,
		              size_t bytes = 0)
		: fis(range), atapi_cmd({range.start + ATAPI_CMD_OFF, range.num_bytes - ATAPI_CMD_OFF}),
		  prdt({range.start + PRDT_OFF, range.num_bytes - PRDT_OFF}, phys, bytes)
		{ }

		static constexpr size_t size() { return 0x100; }
	};
} /* namespace Ahci */

/**
 * Minimalistic AHCI port structure to merely detect device signature
 */
struct Ahci::Port_base
{
	/* device signature */
	enum Signature {
		ATA_SIG        = 0x101,
		ATAPI_SIG      = 0xeb140101,
		ATAPI_SIG_QEMU = 0xeb140000, /* will be fixed in Qemu */
	};

	/**
	 * Port signature
	 */
	struct Mmio_port : public Mmio<0x28> {
		Mmio_port(Byte_range_ptr const &range) : Mmio<0x28>(range) { }

		struct Sig : Register<0x24, 32> { };
	};

	unsigned             const  index;
	Platform::Connection       &plat;
	Hba                        &hba;
	Mmio_port::Delayer         &delayer;

	static constexpr addr_t offset() { return 0x100; }
	static constexpr size_t size()   { return 0x80;  }

	Port_base(unsigned index, Platform::Connection &plat, Hba &hba,
	          Mmio_port::Delayer &delayer)
	:
	  index(index), plat(plat), hba(hba), delayer(delayer) { }

	void with_mmio_port(auto const &fn) const
	{
		hba.with_mmio([&](Hba_mmio &hba_mmio) {
			Mmio_port const mmio(hba_mmio.range_at(offset() + (index * size())));

			fn(mmio);
		}, [&]() { error("with_mmio_port failed"); });
	}

	bool implemented() const
	{
		return hba.port_implemented(index);
	}

	bool ata() const
	{
		bool ata = false;

		with_mmio_port([&](Mmio_port const &mmio) {
			ata = mmio.read<Mmio_port::Sig>() == ATA_SIG; });

		return ata;
	}

	bool atapi() const
	{
		unsigned sig = 0;

		with_mmio_port([&](Mmio_port const &mmio) {
			sig = mmio.read<Mmio_port::Sig>();
		});

		return sig == ATAPI_SIG || sig == ATAPI_SIG_QEMU;
	}
};


struct Ahci::Protocol : Interface
{
	virtual unsigned             init(Port &, Port_mmio &) = 0;
	virtual Block::Session::Info info() const = 0;
	virtual Response             submit(Port &, Block::Request const &, Port_mmio &) = 0;
	virtual Block::Request       completed(Port_mmio &) = 0;
	virtual void                 handle_irq(Port &, Port_mmio &) = 0;
	virtual void                 writeable(bool rw) = 0;
	virtual bool                 pending_requests() const = 0;
};


struct Ahci::Port_mmio : public Mmio<0x3c> {
	Port_mmio(Byte_range_ptr const &range) : Mmio<0x3c>(range) { }

	/**
	 * Command list base (1K length naturally aligned)
	 */
	struct Clb  : Register<0x0, 32> { };

	/**
	 * Command list base upper 32 bit
	 */
	struct Clbu : Register<0x4, 32> { };

	/**
	 * FIS base address (256 bytes naturally aligned)
	 */
	struct Fb : Register<0x8, 32> { };

	/**
	 * FIS base address upper 32 bit
	 */
	struct Fbu : Register<0xc, 32> { };

	/**
	 * Port interrupt status
	 */
	struct Is : Register<0x10, 32, 1>
	{
		struct Dhrs : Bitfield< 0, 1> { }; /* device to host register FIS */
		struct Pss  : Bitfield< 1, 1> { }; /* PIO setup FIS */
		struct Dss  : Bitfield< 2, 1> { }; /* DMA setup FIS */
		struct Sdbs : Bitfield< 3, 1> { }; /* Set device bit  */
		struct Pcs  : Bitfield< 6, 1> { }; /* port connect change status */
		struct Prcs : Bitfield<22, 1> { }; /* PhyRdy change status */
		struct Infs : Bitfield<26, 1> { }; /* interface non-fatal error */
		struct Ifs  : Bitfield<27, 1> { }; /* interface fatal error */

		/* ncq irq */
		struct Fpdma_irq   : Sdbs { };

		/* non-ncq irq */
		struct Dma_ext_irq : Bitfield<0, 3> { };
	};

	/**
	 * Port interrupt enable
	 */
	struct Ie : Register<0x14, 32, 1>
	{
		struct Dhre : Bitfield< 0, 1> { }; /* device to host register FIS interrupt */
		struct Pse  : Bitfield< 1, 1> { }; /* PIO setup FIS interrupt */
		struct Dse  : Bitfield< 2, 1> { }; /* DMA setup FIS interrupt */
		struct Sdbe : Bitfield< 3, 1> { }; /* set device bits FIS interrupt (ncq) */
		struct Ufe  : Bitfield< 4, 1> { }; /* unknown FIS */
		struct Dpe  : Bitfield< 5, 1> { }; /* descriptor processed */
		struct Ifne : Bitfield<26, 1> { }; /* interface non-fatal error */
		struct Ife  : Bitfield<27, 1> { }; /* interface fatal error */
		struct Hbde : Bitfield<28, 1> { }; /* host bus data error */
		struct Hbfe : Bitfield<29, 1> { }; /* host bus fatal error */
		struct Tfee : Bitfield<30, 1> { }; /* task file error */
	};

	/**
	 * Port command
	 */
	struct Cmd : Register<0x18, 32>
	{
		struct St    : Bitfield< 0, 1> { }; /* start */
		struct Sud   : Bitfield< 1, 1> { }; /* spin-up device */
		struct Pod   : Bitfield< 2, 1> { }; /* power-up device */
		struct Fre   : Bitfield< 4, 1> { }; /* FIS receive enable */
		struct Fr    : Bitfield<14, 1> { }; /* FIS receive running */
		struct Cr    : Bitfield<15, 1> { }; /* command list running */
		struct Atapi : Bitfield<24, 1> { };
		struct Icc   : Bitfield<28, 4> { }; /* interface communication control */
	};

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

	/**
	 * Serial ATA control
	 */
	struct Sctl : Register<0x2c, 32>
	{
		struct Det  : Bitfield<0, 4> { }; /* device detection initialization */
		struct Ipmt : Bitfield<8, 4> { }; /* allowed power management transitions */
	};

	/**
	 * Serial ATA active (strict write)
	 */
	struct Sact : Register<0x34, 32, true> { };

	/**
	 * Command issue (strict write)
	 */
	struct Ci : Register<0x38, 32, true> { };
};


/**
 * AHCI port
 */
struct Ahci::Port : private Port_base
{
	using Port_base::index;
	using Port_base::hba;
	using Port_base::delayer;
	using Port_base::plat;

	struct Not_ready : Exception { };

	Protocol    &protocol;
	Region_map  &rm;
	unsigned     cmd_slots = hba.command_slots();

	bool         stop_processing { };

	Platform::Dma_buffer device_dma { plat, 0x1000, CACHED };
	Platform::Dma_buffer cmd_dma    { plat,
		align_addr(cmd_slots * Command_table::size(), 12), CACHED };
	Platform::Dma_buffer device_info_dma { plat, 0x1000, CACHED };

	addr_t device_info_dma_addr = 0;

	Constructible<Byte_range_ptr> cmd_list { };
	Constructible<Byte_range_ptr> fis { };
	Constructible<Byte_range_ptr> cmd_table { };
	Constructible<Byte_range_ptr> device_info { };

	Constructible<Platform::Dma_buffer> dma_buffer { };
	addr_t dma_base  = 0; /* physical address of DMA memory */

	void _with_port_mmio(auto const &fn, auto const &fn_error)
	{
		hba.with_mmio([&](Hba_mmio &hba_mmio) {
			auto range = hba_mmio.range_at(offset() + (index * size()));
			Port_mmio mmio(range);

			fn(mmio);
		}, fn_error);
	}

	Port(Protocol &protocol, Region_map &rm, Platform::Connection & plat,
	     Hba &hba, Mmio_port::Delayer &delayer, unsigned index)
	:
		Port_base(index, plat, hba, delayer),
		protocol(protocol), rm(rm)
	{
		reinit();
	}

	void reinit()
	{
		_with_port_mmio([&](Port_mmio &mmio) {
			reset(mmio);

			if (!enable(mmio))
				throw 1;

			stop(mmio);

			mmio.wait_for(delayer, Cmd::Cr::Equal(0));

			init(mmio);

			/*
			 * Init protocol and determine actual number of command slots of device
			 */
			try {
				unsigned const device_slots = protocol.init(*this, mmio);
				cmd_slots                   = min(device_slots, cmd_slots);
			} catch (Polling_timeout) {
				/* ack any pending IRQ from failed device initialization */
				ack_irq(mmio);
				throw;
			}
		}, [&](){ error("Port() failed"); });
	}

	virtual ~Port() { }

	void command_list_base(uint64_t const phys, Port_mmio &mmio)
	{
		mmio.write<Port_mmio::Clb> ((uint32_t)(phys));
		mmio.write<Port_mmio::Clbu>((uint32_t)(phys >> 32));
	}

	void fis_rcv_base(uint64_t const phys, Port_mmio &mmio)
	{
		mmio.write<Port_mmio::Fb> ((uint32_t)(phys));
		mmio.write<Port_mmio::Fbu>((uint32_t)(phys >> 32));
	}

	typedef Port_mmio::Is   Is;
	typedef Port_mmio::Ie   Ie;
	typedef Port_mmio::Cmd  Cmd;
	typedef Port_mmio::Tfd  Tfd;
	typedef Port_mmio::Ssts Ssts;
	typedef Port_mmio::Serr Serr;
	typedef Port_mmio::Sctl Sctl;
	typedef Port_mmio::Sact Sact;
	typedef Port_mmio::Ci   Ci;

	typedef Port_mmio::Register_set::Polling_timeout Polling_timeout;
	typedef Port_mmio::Register_set::Attempts        Attempts;
	typedef Port_mmio::Register_set::Microseconds    Microseconds;


	void ack_irq(Port_mmio &mmio)
	{
		auto const status = mmio.read<Is>();

		/* clear Serr.Diag.x */
		if (Is::Pcs::get(status))
			mmio.write<Serr::Diag::X>(0);

		/* clear Serr.Diag.n */
		if (Is::Prcs::get(status))
			mmio.write<Serr::Diag::N>(0);

		mmio.write<Is>(mmio.read<Is>());
	};

	void interrupt_enable(Port_mmio &mmio)
	{
		Ie::access_t ie = 0;
		Ie::Dhre::set(ie, 1);
		Ie::Pse ::set(ie, 1);
		Ie::Dse ::set(ie, 1);
		Ie::Sdbe::set(ie, 1);
		Ie::Ufe ::set(ie, 1);
		Ie::Dpe ::set(ie, 1);
		Ie::Ifne::set(ie, 1);
		Ie::Hbde::set(ie, 1);
		Ie::Hbfe::set(ie, 1);
		Ie::Tfee::set(ie, 1);
		mmio.write<Ie>(~0U);
	}

	void start(Port_mmio &mmio)
	{
		if (mmio.read<Cmd::St>())
			return;

		try {
			/* wait up to 5s */
			mmio.wait_for(Port::Attempts(5000),
			              Port::Microseconds(1000),
			              delayer,
			              Tfd::Sts_bsy::Equal(0));
		} catch (Polling_timeout) {
			error("HBA busy unable to start command processing.");
			return;
		}

		try {
			mmio.wait_for(delayer, Tfd::Sts_drq::Equal(0));
		} catch (Polling_timeout) {
			error("HBA in DRQ unable to start command processing.");
			return;
		}

		mmio.write<Cmd::St>(1);
	}

	void stop(Port_mmio &mmio)
	{
		if (!(mmio.read<Ci>() | mmio.read<Sact>()))
			mmio.write<Cmd::St>(0);
	}

	void power_up(Port_mmio &mmio)
	{
		Cmd::access_t cmd = mmio.read<Cmd>();
		Cmd::Sud::set(cmd, 1);
		Cmd::Pod::set(cmd, 1);
		Cmd::Fre::set(cmd, 1);
		mmio.write<Cmd>(cmd);
	}

	bool enable(Port_mmio &mmio)
	{
		auto status = mmio.read<Ssts>();
		if (Ssts::Dec::get(status) == Ssts::Dec::NONE)
			return false;

		/* if in power-mgmt state (partial or slumber)  */
		if (Ssts::Ipm::get(status) & Ssts::Ipm::SUSPEND) {
			/* try to wake up device */
			mmio.write<Cmd::Icc>(Ssts::Ipm::ACTIVE);

			retry<Not_ready>(
				[&] {
						if ((Ssts::Dec::get(status) != Ssts::Dec::ESTABLISHED) ||
						    !(Ssts::Ipm::get(status) &  Ssts::Ipm::ACTIVE))
						throw Not_ready();
				},
				[&] {
					delayer.usleep(1000);
					status = mmio.read<Ssts>();
				}, 10);
		}

		return ((Ssts::Dec::get(status) == Ssts::Dec::ESTABLISHED) &&
		        (Ssts::Ipm::get(status) &  Ssts::Ipm::ACTIVE));
	}

	void reset(Port_mmio &mmio)
	{
		if (mmio.read<Cmd::St>())
			warning("CMD.ST bit set during device reset --> unknown behavior");

		mmio.write<Sctl::Det>(1);
		delayer.usleep(1000);
		mmio.write<Sctl::Det>(0);

		try {
			mmio.wait_for(delayer, Ssts::Dec::Equal(Ssts::Dec::ESTABLISHED));
		} catch (Polling_timeout) {
			warning("Port reset failed");
		}
	}

	void clear_serr(Port_mmio &mmio)
	{
		mmio.write<Serr>(mmio.read<Serr>());
	}

	void init(Port_mmio &mmio)
	{
		/* stop command list processing */
		stop(mmio);

		/* setup command list/table */
		setup_memory(mmio);

		/* disallow all power-management transitions */
		mmio.write<Sctl::Ipmt>(0x3);

		/* power-up device */
		power_up(mmio);

		/* reset port */
		reset(mmio);

		/* clean error register */
		clear_serr(mmio);

		/* enable required interrupts */
		interrupt_enable(mmio);

		/* acknowledge all pending interrupts */
		ack_irq(mmio);
		hba.ack_irq();
	}

	void setup_memory(Port_mmio &mmio)
	{
		/* command list 1K */
		addr_t phys = device_dma.dma_addr();

		cmd_list.construct(device_dma.local_addr<char>(), device_dma.size());
		command_list_base(phys, mmio);

		/* receive FIS base 256 byte */
		enum { FIS_OFF = 1024 };
		fis.construct(cmd_list->start + FIS_OFF, cmd_list->num_bytes - FIS_OFF);

		/*
		 *  Set fis receive base, clear Fre (FIS receive) before and wait for FR
		 *  (FIS receive running) to clear
		 */
		mmio.write<Cmd::Fre>(0);
		mmio.wait_for(delayer, Cmd::Fr::Equal(0));
		fis_rcv_base(phys + 1024, mmio);

		/* command table */
		cmd_table.construct(cmd_dma.local_addr<char>(), cmd_dma.size());
		phys      = cmd_dma.dma_addr();

		/* set command table addresses in command list */
		for (unsigned i = 0; i < cmd_slots; i++) {
			off_t off = (i * Command_header::size());
			Command_header h({cmd_list->start + off, cmd_list->num_bytes - off});
			h.cmd_table_base(phys + (i * Command_table::size()));
		}

		/* dataspace for device info */
		device_info_dma_addr = device_info_dma.dma_addr();
		device_info.construct(device_info_dma.local_addr<char>(), device_info_dma.size());
	}

	Byte_range_ptr command_table_range(unsigned slot)
	{
		off_t off = slot * Command_table::size();
		return {cmd_table->start + off, cmd_table->num_bytes - off};
	};

	Byte_range_ptr command_header_range(unsigned slot)
	{
		off_t off = slot * Command_header::size();
		return {cmd_list->start + off, cmd_list->num_bytes - off};
	}

	void execute(unsigned slot, Port_mmio &mmio)
	{
		start(mmio);

		mmio.write<Ci>(1U << slot);
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

	Dataspace_capability alloc_buffer(size_t size)
	{
		if (dma_buffer.constructed()) return Dataspace_capability();

		dma_buffer.construct(plat, size, CACHED);
		dma_base = dma_buffer->dma_addr();

		return dma_buffer->cap();
	}

	void free_buffer()
	{
		if (!dma_buffer.constructed()) return;
		dma_buffer.destruct();
	}

	/**********************
	 ** Protocol wrapper **
	 **********************/


	Block::Session::Info info() const { return protocol.info(); }

	void handle_irq()
	{
		_with_port_mmio([&](Port_mmio &mmio) {
			protocol.handle_irq(*this, mmio);
		}, [&](){ error("Port::handle_irq failed"); });
	}

	Response submit(Block::Request const &request)
	{
		Response response { };

		_with_port_mmio([&](Port_mmio &mmio) {
			response = protocol.submit(*this, request, mmio);
		}, [&](){ error("Port::submit failed"); });

		return response;
	}

	void for_one_completed_request(auto const &fn)
	{
		_with_port_mmio([&](Port_mmio &mmio) {

			Block::Request request = protocol.completed(mmio);
			if (!request.operation.valid()) return;

			request.success = true;
			fn(request);

		}, [&](){
			if (protocol.pending_requests())
				error("for_one_completed_request failed with pending requests");
		});
	}

	void writeable(bool rw) { protocol.writeable(rw); }
};


void Ahci::Hba::ack_irq()
{
	_resource.with_mmio_irq([&](Hba_mmio &mmio, Platform::Device::Irq &irq) {
		mmio.write<Hba_mmio::Is>(mmio.read<Hba_mmio::Is>());
		irq.ack();
	}, [&](){ error("Hba::ack_irq() failed"); });
}


void Ahci::Hba::with_mmio(auto const &fn, auto const &fn_error)
{
	_resource.with_mmio(fn, fn_error);
}


void Ahci::Hba::with_mmio(auto const &fn, auto const &fn_error) const
{
	_resource.with_mmio(fn, fn_error);
}

#endif /* _AHCI__AHCI_H_ */
