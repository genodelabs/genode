/*
 * \brief  AHCI driver implementation
 * \author Martin Stein <martin.stein@genode-labs.com>
 * \date   2013-05-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <ahci_driver.h>

/* Genode includes */
#include <os/attached_mmio.h>
#include <timer_session/connection.h>
#include <irq_session/connection.h>
#include <util/mmio.h>
#include <util/string.h>
#include <dataspace/client.h>

using namespace Genode;

/**
 * Delayer for MMIO polling
 */
struct Timer_delayer : Mmio::Delayer, Timer::Connection
{
	void usleep(unsigned us) { Timer::Connection::usleep(us); }
};

static Mmio::Delayer * delayer() {
	static Timer_delayer s;
	return &s;
}

/**
 * Compose a physical region descriptor
 *
 * \param prd_addr  destination
 * \param phys      physical region base
 * \param size      physical region size
 */
void write_prd(addr_t prd_addr, uint64_t phys, unsigned size)
{
	struct Bits : Register<32>
	{
		struct Dbc : Bitfield<0, 22> { }; /* data byte count */
		struct I   : Bitfield<31, 1> { }; /* IRQ on completion */
	};
	struct Prd
	{
		uint64_t dba; /* data base address */
		uint32_t zero;
		uint32_t bits;
	};
	Bits::access_t bits = 0;
	Bits::Dbc::set(bits, size - 1);

	Prd volatile * prd = (Prd volatile *)prd_addr;
	prd->dba  = phys;
	prd->zero = 0;
	prd->bits = bits;
}

/**
 * Compose a command slot
 *
 * \param slot_addr  destination
 * \param ct_phys    physical command table base address
 * \param w          writes 1: host-to-device 0: device-to-host
 * \param reset      wether it is a soft reset command
 * \param pmp        port multiplier port
 * \param prdtl      physical region descriptor table length in entries
 */
void write_cmd_slot(addr_t slot_addr, uint64_t ct_phys, bool w,
                    bool reset, uint8_t pmp, uint16_t prdtl)
{
	struct Bits : Register<32>
	{
		struct Cfl   : Bitfield<0, 5>   { }; /* command FIS length */
		struct A     : Bitfield<5, 1>   { }; /* ATAPI command */
		struct W     : Bitfield<6, 1>   { }; /* write (1: H2D, 0: D2H) */
		struct P     : Bitfield<7, 1>   { }; /* prefetchable */
		struct R     : Bitfield<8, 1>   { }; /* reset */
		struct B     : Bitfield<9, 1>   { }; /* BIST */
		struct C     : Bitfield<10, 1>  { }; /* clear busy upon R_OK */
		struct Pmp   : Bitfield<12, 4>  { }; /* port multiplier port */
		struct Prdtl : Bitfield<16, 16> { }; /* PRD-table length in entries */
	};
	struct Slot
	{
		uint32_t bits;
		uint32_t prdbc; /* PRD byte count */
		uint64_t ctba;  /* command table descriptor base address */
		uint32_t zero;
	};
	Bits::access_t bits = 0;
	Bits::Cfl::set(bits, 5);
	Bits::W::set(bits, w);
	Bits::R::set(bits, reset);
	Bits::C::set(bits, reset);
	Bits::Pmp::set(bits, pmp);
	Bits::Prdtl::set(bits, prdtl);

	Slot volatile * slot = (Slot volatile *)slot_addr;
	slot->bits           = bits;
	slot->prdbc          = 0;
	slot->ctba           = ct_phys;
	slot->zero           = 0;
}

/**
 * Frame information structure
 */
struct Fis
{
	/* FIS payload */
	uint8_t volatile byte[20];

	void _init()
	{
		for (unsigned i = 0; i < sizeof(byte)/sizeof(byte[0]); i++)
			byte[i] = 0;
	}

	void _reg_h2d()
	{
		byte[0]  = 0x27; /* type */
		byte[15] = 0x08; /* control */
	}

	void _cmd_h2d()
	{
		_reg_h2d();

		struct Flags : Register<8>
		{
			struct Cmd : Bitfield<7, 1> { };  /* 1: command 0: control */
		};
		Flags::access_t flags = 0;
		Flags::Cmd::set(flags, 1);

		byte[1]  = flags;
	}

	void _obsolete_device()
	{
		byte[7]  = 0xa0;
	}

	void _lba(uint64_t lba)
	{
		byte[4]  =  lba        & 0xff;
		byte[5]  = (lba >> 8)  & 0xff;
		byte[6]  = (lba >> 16) & 0xff;
		byte[8]  = (lba >> 24) & 0xff;
		byte[9]  = (lba >> 32) & 0xff;
		byte[10] = (lba >> 40) & 0xff;
	}

	void _feature(uint16_t ft)
	{
		byte[3]  = ft & 0xff;
		byte[11] = (ft >> 8) & 0xff;
	}

	void _count(uint16_t cnt)
	{
		byte[12] = cnt & 0xff;
		byte[13] = (cnt >> 8) & 0xff;
	}

	public:

		/**
		 * Read PIO-setup transfer count
		 */
		uint16_t transfer_cnt()
		{
			uint16_t ret = 0;
			ret |= (uint16_t)byte[16];
			ret |= (uint16_t)byte[17] << 8;
			return ret;
		}

		/**
		 * Read count
		 */
		uint16_t count()
		{
			uint16_t ret = 0;
			ret |= (uint16_t)byte[12];
			ret |= (uint16_t)byte[13] << 8;
			return ret;
		}

		/**
		 * Read logical block address
		 */
		uint64_t lba()
		{
			uint64_t ret = 0;
			ret |= (uint64_t)byte[4];
			ret |= (uint64_t)byte[5] << 8;
			ret |= (uint64_t)byte[6] << 16;
			ret |= (uint64_t)byte[8] << 24;
			ret |= (uint64_t)byte[9] << 32;
			ret |= (uint64_t)byte[10] << 40;
			return ret;
		}

		/**
		 * FIS to clear device-to-host receive area
		 */
		void clear_d2h_rx()
		{
			_init();
			_reg_h2d();
			_obsolete_device();

			byte[2] = 0x80; /* command */
		}

		/**
		 * Command FIS for ATA command 'identify device'
		 */
		void identify_device()
		{
			_init();
			_cmd_h2d();
			_obsolete_device();

			byte[2] = 0xec; /* command */
		}

		/**
		 * Command FIS for ATA command 'read native max addr'
		 */
		void read_native_max_addr()
		{
			_init();
			_cmd_h2d();
			_obsolete_device();

			byte[2]  = 0x27; /* command */
			byte[7] |= 0x40; /* device */
		}

		/**
		 * Command FIS for ATA command 'set features' with feature 'set transfer mode'
		 *
		 * \param transfer_mode  ID of targeted mode
		 */
		void set_transfer_mode(uint8_t transfer_mode)
		{
			_init();
			_cmd_h2d();
			_obsolete_device();
			_feature(3);
			_count(transfer_mode);

			byte[2] = 0xef; /* command */
		}

		/**
		 * Command FIS for ATA command 'read / write FPDMA queued'
		 *
		 * \param w          1: do write FPDMA queued 0: do read FPDMA queued
		 * \param block_nr   logical block address (LBA)
		 * \param block_cnt  blocks to be read / write
		 * \param tag        command slot ID
		 */
		void fpdma_queued(bool w, uint64_t block_nr,
		                  uint16_t block_cnt, unsigned tag)
		{
			struct Count : Register<16>
			{
				struct Tag : Bitfield<3, 5> { };
			};
			Count::access_t cnt = 0;
			Count::Tag::set(cnt, tag);

			struct Device : Register<8>
			{
				struct Fua : Bitfield<7, 1> { };
			};
			Device::access_t dev = 0x40;

			_init();
			_cmd_h2d();
			_feature(block_cnt);
			_lba(block_nr);
			_count(cnt);

			byte[2] = w ? 0x61 : 0x60; /* command */
			byte[7] = dev;
		}

		/**
		 * First and second soft reset FIS
		 *
		 * \param second  if this is the second soft reset FIS or the first
		 * \param pmp     port multiplier port
		 */
		void soft_reset(bool second, uint8_t pmp)
		{
			_init();
			_reg_h2d();
			_obsolete_device();

			struct Control : Register<8>
			{
				struct Softreset : Bitfield<2, 1> { };
			};
			Control::access_t ctl = byte[15];
			Control::Softreset::set(ctl, !second);

			struct Flags : Register<8>
			{
				struct Pmp : Bitfield<0, 4> { };  /* port multiplier port */
			};
			Flags::access_t flags = 0;
			Flags::Pmp::set(flags, pmp);

			byte[1]  = flags;
			byte[15] = ctl;
		}

		/**
		 * Wether a PIO setup FIS was sucessfully received
		 *
		 * \param transfer_size  size of transfered data
		 * \param block_nr       LBA of transfered data (0 if it has no LBA)
		 */
		bool is_pio_setup(uint16_t transfer_size, uint64_t block_nr)
		{
			struct Flags : Register<8>
			{
				struct Pmp : Bitfield<0,4> { }; // port multiplier port
				struct D   : Bitfield<5,1> { }; // data transfer direction, 1: D2H
				struct I   : Bitfield<6,1> { }; // interrupt bit
			};
			Flags::access_t flags = 0;
			Flags::D::set(flags, 1);
			Flags::I::set(flags, 1);

			/*
			 * FIXME
			 * The count register is set differently for different
			 * drives and i've no idea what it means in this context
			 * but as long as all works fine i ignore it simply.
			 * (WD2500BEVS: 0xff, SAMSUNG840PRO128GB: 0x1)
			 */
			return byte[0]        == 0x5f     && /* type */
			       byte[1]        == flags    &&
			       byte[2]        == 0x58     && /* old status */
			       byte[3]        == 0        && /* error */
			       lba()          == block_nr &&
			       byte[7]        == 0xa0     && /* device */
			       byte[15]       == 0x50     && /* new status */
			       transfer_cnt() == transfer_size;
		}

		/**
		 * Wether reply to cmd. 'set transfer mode' was successfully received
		 *
		 * \param transfer_mode  ID of transfer mode that should be set
		 */
		bool is_set_transfer_mode_reply(uint8_t transfer_mode)
		{

			/*
			 * FIXME
			 * I've no idea what most of these values stand for and
			 * interpreting Linux seems to be the only way to change this.
			 */
			bool result = 0;
			result = byte[0]  == 0x34 && /* type */
			         byte[1]  == 0x40 &&
			         byte[2]  == 0x50 &&
			         byte[3]  == 0    &&
			         lba()    == 0    &&
			         byte[7]  == 0xa0 && /* device */
			         byte[11] == 0    &&
			         byte[14] == 0    &&
			         byte[15] == 0;

			/*
			 * FIXME
			 * Sometimes count is 0 and sometimes it equals the transfer
			 * mode that was set but both seems to work.
			 */
			if (count() == 0)
				printf("cleared transfer mode in reconfiguration reply\n");
			else if (count() != transfer_mode)
				result = 0;

			return result;
		}
};

/**
 * Clock management unit
 */
struct Cmu
{
	/**
	 * Top part of the CMU
	 */
	struct Top : Attached_mmio
	{
		/********************************
		 ** MMIO structure description **
		 ********************************/

		struct Clk_src_mask_fsys : Register<0x340, 32>
		{
			struct Sata_mask : Bitfield<24, 1> { };
		};
		struct Clk_div_fsys0 : Register<0x548, 32>
		{
			struct Sata_ratio : Bitfield<20, 4> { };
		};
		struct Clk_gate_ip_fsys : Register<0x944, 32>
		{
			struct Pdma0         : Bitfield<1, 1> { };
			struct Pdma1         : Bitfield<2, 1> { };
			struct Sata          : Bitfield<6, 1> { };
			struct Sata_phy_ctrl : Bitfield<24, 1> { };
			struct Sata_phy_i2c  : Bitfield<25, 1> { };
		};

		/**
		 * Constructor
		 */
		Top() : Attached_mmio(0x10020000, 0x10000) { }

		/**
		 * Switch clock enable for SATA PHY I2C interface
		 *
		 * \param on  switch
		 */
		void sata_phy_i2c(bool on) {
			write<Clk_gate_ip_fsys::Sata_phy_i2c>(on); }

		/**
		 * Initialize clock config for SATA components
		 */
		void init_sata()
		{
			/* PDMA */
			write<Clk_gate_ip_fsys::Pdma0>(1);
			write<Clk_gate_ip_fsys::Pdma1>(1);

			/* SATA PHY CTRL & SATA AHCI */
			write<Clk_src_mask_fsys::Sata_mask>(1);
			write<Clk_div_fsys0::Sata_ratio>(12);
			write<Clk_gate_ip_fsys::Sata>(1);
			write<Clk_gate_ip_fsys::Sata_phy_ctrl>(1);
		}
	} top;
};

static Cmu * cmu() {
	static Cmu cmu;
	return &cmu;
}

/**
 * Power management unit
 */
struct Pmu : Attached_mmio
{
	struct Sata_phy_control : Register<0x724, 32>
	{
		struct Enable : Bitfield<0, 1> { };
	};

	/**
	 * Constructor
	 */
	Pmu() : Attached_mmio(0x10040000, 0x10000) { }

	/**
	 * Power on SATA components
	 */
	void init_sata() { write<Sata_phy_control::Enable>(1); }
};

static Pmu * pmu() {
	static Pmu pmu;
	return &pmu;
}

/**
 * SATA AHCI interface
 */
struct Sata_ahci : Attached_mmio
{
	enum { VERBOSE = 1 };

	/* general config */
	enum {
		/* FIXME only with port multiplier support (sata_srst_pmp in Linux) */
		SOFT_RESET_PMP   = 15,
		BLOCK_SIZE       = 512,
		BLOCKS_PER_LOG   = 1,
		BYTES_PER_PRD    = 1 << 22,
	};

	/* DMA structure */
	enum {
		CMD_LIST_SIZE           = 0x400,
		CMD_SLOT_SIZE           = 0x20,
		FIS_AREA_SIZE           = 0x100,
		CMD_TABLE_SIZE          = 0xb00,
		CMD_TABLE_HEAD_SIZE     = 0x80,
		PRD_SIZE                = 0x10,
	};

	/* FIS RX area structure */
	enum {
		REG_D2H_FIS_OFFSET  = 0x40,
		PIO_SETUP_FIS_OFFSET = 0x20,
	};

	/* modes when doing 'set features' with feature 'set transfer mode' */
	enum { UDMA_133 = 0x46, };

	/********************************
	 ** MMIO structure description **
	 ********************************/

	struct Cap : Register<0x0, 32>
	{
		struct Np  : Bitfield<0, 4> { };
		struct Ems : Bitfield<6, 1> { };
		struct Ncs : Bitfield<8, 5> { };
		struct Iss : Bitfield<20, 4> { };
	};
	struct Ghc : Register<0x4, 32>
	{
		struct Hr : Bitfield<0, 1> { };
		struct Ie : Bitfield<1, 1> { };
		struct Ae : Bitfield<31, 1> { };
	};
	struct Is : Register<0x8, 32, 1>
	{
		struct Ips : Bitfield<0, 1> { };
	};
	struct Pi : Register<0xc, 32> { };
	struct Vs : Register<0x10, 32>
	{
		struct Mnr : Bitfield<0, 16> { };
		struct Mjr : Bitfield<16, 16> { };
	};
	struct Cap2  : Register<0x24, 32> { };
	struct P0clb : Register<0x100, 32>
	{
		struct Clb : Bitfield<10, 22> { };
	};
	struct P0fb : Register<0x108, 32>
	{
		struct Fb : Bitfield<8, 24> { };
	};
	struct P0is : Register<0x110, 32, 1>
	{
		struct Dhrs : Bitfield<0, 1> { };
		struct Pss  : Bitfield<1, 1> { };
		struct Sdbs : Bitfield<3, 1> { };
		struct Infs : Bitfield<26, 1> { };
		struct Ifs  : Bitfield<27, 1> { };
	};
	struct P0ie : Register<0x114, 32>
	{
		struct Dhre : Bitfield<0, 1> { };
		struct Pse  : Bitfield<1, 1> { };
		struct Dse  : Bitfield<2, 1> { };
		struct Sdbe : Bitfield<3, 1> { };
		struct Ufe  : Bitfield<4, 1> { };
		struct Dpe  : Bitfield<5, 1> { };
		struct Pce  : Bitfield<6, 1> { };
		struct Prce : Bitfield<22, 1> { };
		struct Ife  : Bitfield<27, 1> { };
		struct Hbde : Bitfield<28, 1> { };
		struct Hbfe : Bitfield<29, 1> { };
		struct Tfee : Bitfield<30, 1> { };
	};
	struct P0cmd : Register<0x118, 32>
	{
		struct St    : Bitfield<0, 1> { };
		struct Sud   : Bitfield<1, 1> { };
		struct Pod   : Bitfield<2, 1> { };
		struct Fre   : Bitfield<4, 1> { };
		struct Fr    : Bitfield<14, 1> { };
		struct Cr    : Bitfield<15, 1> { };
		struct Pma   : Bitfield<17, 1> { };
		struct Atapi : Bitfield<24, 4> { };
		struct Icc   : Bitfield<28, 4> { };
	};
	struct P0tfd : Register<0x120, 32>
	{
		struct Sts_bsy : Bitfield<7, 1> { };
	};
	struct P0sig : Register<0x124, 32>
	{
		struct Lba_8_15  : Bitfield<16, 8> { };
		struct Lba_16_31 : Bitfield<24, 8> { };
	};
	struct P0ssts : Register<0x128, 32>
	{
		struct Det : Bitfield<0, 4> { };
		struct Spd : Bitfield<4, 4> { };
		struct Ipm : Bitfield<8, 4> { };
	};
	struct P0sctl : Register<0x12c, 32>
	{
		struct Det : Bitfield<0, 4> { };
		struct Spd : Bitfield<4, 4> { };
		struct Ipm : Bitfield<8, 4> { };
	};
	struct P0serr : Register<0x130, 32>
	{
		struct Err_c  : Bitfield<9, 1> { };
		struct Err_p  : Bitfield<10, 1> { };
		struct Diag_b : Bitfield<19, 1> { };
		struct Diag_c : Bitfield<21, 1> { };
		struct Diag_h : Bitfield<22, 1> { };
	};
	struct P0sact : Register<0x134, 32, 1> { };
	struct P0ci   : Register<0x138, 32, 1> { };
	struct P0sntf : Register<0x13c, 32, 1>
	{
		struct Pmn : Bitfield<0, 16> { };
	};

	Irq_connection       irq;     /* port 0 IRQ */
	uint64_t             block_cnt;
	Dataspace_capability ds;      /* working DMA */
	addr_t               cl_phys; /* command list */
	addr_t               cl_virt;
	addr_t               fb_phys; /* FIS receive area */
	addr_t               fb_virt;
	addr_t               ct_phys; /* command table */
	addr_t               ct_virt;

	/**
	 * Constructor
	 */
	Sata_ahci()
	: Attached_mmio(0x122f0000, 0x10000),
	  irq(147), ds(env()->ram_session()->alloc(0x20000, 0)),
	  cl_phys(Dataspace_client(ds).phys_addr()),
	  cl_virt(env()->rm_session()->attach(ds)),
	  fb_phys(cl_phys + CMD_LIST_SIZE),
	  fb_virt(cl_virt + CMD_LIST_SIZE),
	  ct_phys(fb_phys + FIS_AREA_SIZE),
	  ct_virt(fb_virt + FIS_AREA_SIZE)
	{ }

	/**
	 * Clear all interrupts at port 0
	 *
	 * \return  value of P0IS before it was cleared
	 */
	P0is::access_t p0_clear_irqs()
	{
		P0is::access_t p0is = read<P0is>();
		write<P0is>(p0is);
		return p0is;
	}

	/**
	 * Evaluate port interrupt states and according errors after a command
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_interpret_irqs()
	{
		/* ack interrupt states */
		P0is::access_t p0is = p0_clear_irqs();
		if (P0is::Sdbs::bits(1) == p0is) return 0;

		/* interpret P0IS */
		bool if_error, fatal;
		if (P0is::Ifs::get(p0is)) {
			if_error = 1;
			fatal = 1;
		} else if (P0is::Infs::get(p0is)) {
			if_error = 1;
			fatal = 0;
		} else if_error = 0;

		/* analyse P0SERR if there's an interface error */
		if (if_error) {
			printf("%s ", fatal ? "Fatal" : "Non-fatal");
			P0serr::access_t p0serr = p0_clear_errors();
			if (P0serr::Diag_b::get(p0serr)) {
				printf("10 B to 8 B decode error\n");
				return -1;
			}
			if (P0serr::Err_p::get(p0serr)) {
				printf("protocol error\n");
				return -2;
			}
			if (P0serr::Diag_c::get(p0serr)) {
				printf("CRC error\n");
				return -3;
			}
			if (P0serr::Err_c::get(p0serr)) {
				printf("non-recovered persistent communication error\n");
				return -4;
			}
			if (P0serr::Diag_h::get(p0serr)) {
				printf("handshake error\n");
				return -5;
			}
			printf("unknown interface error\n");
			return -6;
		}
		printf("Unknown error (P0is 0x%x)\n", p0is);
		return -7;
	}

	/**
	 * Get the AHCI controller ready for port initializations
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int init()
	{
		/* enable AHCI */
		write<Ghc::Ae>(1);
		if (!read<Ghc::Ae>()) {
			PERR("SATA AHCI failed to enable AHCI");
			return -1;
		}
		/* save HBA config */
		Cap::access_t  cap  = read<Cap>();
		Pi::access_t   pi   = read<Pi>();
		Vs::access_t   vs   = read<Vs>();
		Cap2::access_t cap2 = read<Cap2>();

		/* check port number and mask */
		unsigned ports     = 0;
		for (unsigned i = 0; i < Pi::ACCESS_WIDTH; i++) if (pi & (1 << i)) ports++;
		if (ports != Cap::Np::get(cap) + 1) {
			ports = Cap::Np::get(cap) + 1;
			pi = (1 << ports) - 1;
		}
		if (ports != 1 || pi != 1) {
			PERR("SATA AHCI driver proved with port 0 only");
			return -1;
		}
		/* check enclosure management support */
		if (Cap::Ems::get(cap)) {
			PERR("SATA AHCI driver proved without EMS only");
			return -1;
		}
		/* check AHCI revision */
		unsigned rev_mjr = Vs::Mjr::get(vs);
		unsigned rev_mnr = Vs::Mnr::get(vs);
		if (rev_mjr != 0x1 || rev_mnr != 0x300) {
			PERR("SATA AHCI driver proved with AHCI rev 1.3 only");
			return -1;
		}
		/* check interface speed */
		char const * speed;
		bool speed_support = 0;
		switch(Cap::Iss::get(cap)) {
		case 1:  speed = "1.5";
		         break;
		case 2:  speed = "3";
		         break;
		case 3:  speed = "6";
		         speed_support = 1;
		         break;
		default: speed = "?";
		}
		if (!speed_support) {
			PERR("SATA AHCI driver not proved with %s Gbps", speed);
			return -1;
		}
		/* check number of command slots */
		unsigned slots = Cap::Ncs::get(cap) + 1;
		if (slots != 32) {
			PERR("SATA AHCI driver proved with 32 slots only");
			return -1;
		}
		/* reset */
		write<Ghc::Hr>(1);
		if (!wait_for<Ghc::Hr>(0, *delayer(), 1000, 1000)) {
			PERR("SATA AHCI reset failed");
			return -1;
		}
		/* enable AHCI */
		write<Ghc::Ae>(1);
		if (!read<Ghc::Ae>()) {
			PERR("SATA AHCI failed to enable AHCI");
			return -1;
		}
		/* restore HBA config */
		write<Cap>(cap);
		write<Cap2>(cap2);
		write<Pi>(pi);
		if (VERBOSE)
			printf("SATA AHCI initialized, AHCI rev %x.%x, "
			       "%s Gbps, %u slots, %u port%c\n",
			       rev_mjr, rev_mnr, speed, slots,
			       ports, ports > 1 ? 's' : ' ');
		return 0;
	}

	/**
	 * Stop processing commands at port 0
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_disable_cmd_processing()
	{
		P0cmd::access_t p0cmd = read<P0cmd>();
		if (P0cmd::St::get(p0cmd) || P0cmd::Cr::get(p0cmd)) {
			write<P0cmd::St>(0);
			if (!wait_for<P0cmd::Cr>(0, *delayer(), 500, 1000)) {
				PERR("PORT0 failed to stop HBA processing");
				return -1;
			}
		}
		return 0;
	}

	/**
	 * Start processing commands at port 0
	 */
	void p0_enable_cmd_processing()
	{
		write<P0cmd::St>(1);
		read<P0cmd>(); /* flush */
	}

	/**
	 * Stop and restart processing commands at port 0
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_restart_cmd_processing()
	{
		if (p0_disable_cmd_processing()) return -1;
		p0_enable_cmd_processing();
		return 0;
	}

	/**
	 * Execute prepared command, wait for completion and acknowledge at port
	 *
	 * \param P0IS_BIT  state bit of the interrupt that's expected to be raised
	 * \param tag       command slot ID
	 *
	 * \retval  0  call was successful
	 * \retval -1  call failed
	 */
	template <typename P0IS_BIT>
	int p0_issue_cmd(unsigned tag)
	{
		typedef typename P0IS_BIT::Bitfield_base P0is_bit;
		write<P0ci>(1 << tag);
		irq.wait_for_irq();
		if (!read<Is::Ips>()) {
			PERR("ATA0 no IRQ raised");
			return -1;
		}
		if (read<P0is>() != P0is_bit::bits(1)) {
			PERR("ATA0 expected P0IS to be %x (is %x)",
			     P0is_bit::bits(1), read<P0is>());
			return -1;
		}
		write<P0is_bit>(1);
		if (read<P0ci>()) {
			PERR("ATA0 unfinished IRQ after command");
			return -1;
		}
		return 0;
	}

	/**
	 * Request and read out the identification data of the port 0 device
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_identify_device()
	{
		/**
		 * Device identification data
		 */
		struct Device_id
		{
			enum {
				/* FIXME use register framework to do shifts */
				UDMA_133_SUPPORTED = 1 << 6,
				UDMA_133_ACTIVE    = 1 << 14,
				SIZE               = 0x200,
			};

			uint16_t na_0[23];          /* word   0.. 22 */
			char     revision[8];       /* word  23.. 26 */
			char     model_nr[40];      /* word  27.. 46 */
			uint16_t na_1[28];          /* word  47.. 74 */
			uint16_t queue_depth;       /* word  75      */
			uint16_t sata_caps;         /* word  76      */
			uint16_t na_2[11];          /* word  77.. 87 */
			uint16_t udma;              /* word  88      */
			uint16_t na_3[11];          /* word  89.. 99 */
			uint64_t total_lba_sectors; /* word 100      */

			/**
			 * Helper to print interchanged char arrays
			 */
			void print(char const * src, size_t size) {
				for(unsigned i = 0; i < size; i+=2)
				{
					if (!src[i+1] || !src[i]) return;
					if (src[i+1] == 0x20 && src[i] == 0x20) return;
					printf("%c%c", src[i+1], src[i]);
				}
			}

			/**
			 * Print model name and firmware revision of the device
			 */
			void print_label()
			{
				print(model_nr, sizeof(model_nr)/sizeof(model_nr[0]));
				printf(" rev ");
				print(revision, sizeof(revision)/sizeof(revision[0]));
			}

			/**
			 * Wether device supports native command queueing (NCQ)
			 */
			bool supports_ncq() { return sata_caps & (1 << 8); }
		};

		/* create receive buffer DMA */
		Ram_dataspace_capability dev_id_ds = env()->ram_session()->alloc(0x1000, 0);;
		addr_t dev_id_virt = (addr_t)env()->rm_session()->attach(dev_id_ds);
		addr_t dev_id_phys = Dataspace_client(dev_id_ds).phys_addr();

		/* do command 'identify device' */
		unsigned tag = 31;
		addr_t   cmd_table = ct_virt + tag * CMD_TABLE_SIZE;
		Fis * fis = (Fis *)cmd_table;
		fis->identify_device();
		unsigned prd_id = 0;
		addr_t prd = cmd_table + CMD_TABLE_HEAD_SIZE + prd_id * PRD_SIZE;
		write_prd(prd, dev_id_phys, Device_id::SIZE);
		addr_t cmd_slot = cl_virt + tag * CMD_SLOT_SIZE;
		write_cmd_slot(cmd_slot, ct_phys + tag * CMD_TABLE_SIZE, 0, 0, 0, 1);
		if (p0_issue_cmd<P0is::Pss>(tag)) return -1;

		/* check if we received the requested data */
		fis = (Fis *)(fb_virt + PIO_SETUP_FIS_OFFSET);
		if (!fis->is_pio_setup(Device_id::SIZE, 0)) {
			PERR("Invalid PIO setup FIS");
			return -1;
		}
		/* interpret device ID */
		Device_id * dev_id = (Device_id *)dev_id_virt;
		block_cnt = dev_id->total_lba_sectors;
		if (VERBOSE) {
			printf("ATA0 ");
			dev_id->print_label();
			printf(", %llu blocks, %llu GB\n", block_cnt,
			       ((uint64_t)block_cnt * BLOCK_SIZE) / 1000000000);
		}
		/* get command mode */
		if (!dev_id->supports_ncq()) {
			PERR("ATA0 driver not proved with modes other than NCQ");
			return -1;
		}
		/* get transfer mode */
		if (!(dev_id->udma & Device_id::UDMA_133_SUPPORTED)) {
			PERR("ATA0 driver not proved with other modes than UDMA133");
			return -1;
		}
		if (VERBOSE)
			printf("ATA0 supports UDMA-133 and NCQ with queue depth %u\n",
			       dev_id->queue_depth + 1);
		write<Is::Ips>(1);

		/* destroy receive buffer DMA */
		env()->rm_session()->detach(dev_id_virt);
		env()->ram_session()->free(dev_id_ds);;
		return 0;
	}

	/**
	 * Wether the port 0 device hides blocks via the HPA feature
	 */
	bool p0_hides_blocks()
	{
		/* do command 'read native max addr' */
		unsigned tag = 31;
		addr_t cmd_table = ct_virt + tag * CMD_TABLE_SIZE;
		Fis * fis = (Fis *)cmd_table;
		fis->read_native_max_addr();
		addr_t cmd_slot = cl_virt + tag * CMD_SLOT_SIZE;
		write_cmd_slot(cmd_slot, ct_phys + tag * CMD_TABLE_SIZE, 0, 0, 0, 0);
		if (p0_issue_cmd<P0is::Dhrs>(tag)) return -1;

		/* read received address */
		fis = (Fis *)(fb_virt + REG_D2H_FIS_OFFSET);
		uint64_t max_native_addr = fis->lba();

		/* end command */
		write<Is::Ips>(1);

		/* check for hidden blocks */
		return max_native_addr + 1 != block_cnt;
	}

	/**
	 * Clear all port errors at port 0
	 *
	 * \return  value of P0SERR before it was cleared
	 */
	P0serr::access_t p0_clear_errors()
	{
		P0serr::access_t const p0serr = read<P0serr>();
		write<P0serr>(p0serr);
		return p0serr;
	}

	/**
	 * Tell port 0 device wich transfer mode to use
	 *
	 * \param mode  ID of targeted transfer mode
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_transfer_mode(uint8_t mode)
	{
		/* do command 'set features' with feature 'set transfer mode' */
		unsigned tag   = 31;
		addr_t cmd_table = ct_virt + tag * CMD_TABLE_SIZE;
		addr_t cmd_slot = cl_virt + tag * CMD_SLOT_SIZE;
		Fis * fis = (Fis *)cmd_table;
		fis->set_transfer_mode(mode);
		write_cmd_slot(cmd_slot, ct_phys + tag * CMD_TABLE_SIZE, 0, 0, 0, 0);
		if (p0_issue_cmd<P0is::Dhrs>(tag)) return -1;

		/* check answer */
		fis = (Fis *)(fb_virt + REG_D2H_FIS_OFFSET);
		if (!fis->is_set_transfer_mode_reply(mode)) {
			PERR("Invalid reply after set up transfer mode");
			return -1;
		}
		/* end command */
		write<Is::Ips>(1);
		return 0;
	}

	/**
	 * Enable interrupt reception for port 0
	 */
	void p0_enable_irqs()
	{
		/* clear IRQs */
		p0_clear_irqs();
		write<Is>(1);

		/* enable all IRQs we need */
		P0ie::access_t p0ie = 0;
		P0ie::Dhre::set(p0ie, 1);
		P0ie::Pse::set(p0ie, 1);
		P0ie::Dse::set(p0ie, 1);
		P0ie::Sdbe::set(p0ie, 1);
		P0ie::Ufe::set(p0ie, 1);
		P0ie::Dpe::set(p0ie, 1);
		P0ie::Pce::set(p0ie, 1);
		P0ie::Prce::set(p0ie, 1);
		P0ie::Ife::set(p0ie, 1);
		P0ie::Hbde::set(p0ie, 1);
		P0ie::Hbfe::set(p0ie, 1);
		P0ie::Tfee::set(p0ie, 1);
		write<P0ie>(p0ie);
	}

	/**
	 * Soft reset link at port 0
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_soft_reset()
	{
		/* first soft reset FIS */
		if (p0_restart_cmd_processing()) return -1;
		Fis * fis = (Fis *)ct_virt;
		fis->soft_reset(0, SOFT_RESET_PMP);
		write_cmd_slot(cl_virt, ct_phys, 0, 1, SOFT_RESET_PMP, 0);

		/* we can't do p0_issue_cmd here - IRQ gets not triggered */
		write<P0ci>(1);
		if (!wait_for<P0ci>(0, *delayer(), 500, 1000)) {
			PERR("ATA0 failed to issue first soft-reset command");
			return -1;
		}
		delayer()->usleep(1000); /* according to spec wait at least 5 us */

		/* second soft reset FIS */
		fis->soft_reset(1, SOFT_RESET_PMP);
		write_cmd_slot(cl_virt, ct_phys, 0, 0, SOFT_RESET_PMP, 0);
		write<P0ci>(1);
		read<P0ci>(); /* this time simply flush because dynamic waiting not needed */

		/* old devices might need 150 ms but newer specs say 2 ms */
		if (!wait_for<P0tfd::Sts_bsy>(0, *delayer(), 75, 2000)) {
			PERR("ATA0 drive hangs in soft reset");
			return -1;
		}
		return 0;
	}

	/**
	 * Hard reset link at port 0
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_hard_reset(bool const set_speed = 0,
	                  P0sctl::access_t const speed = 0)
	{
		enum { IPM = 3 };
		if (set_speed) {
			/*
			 * SATA spec doesn't provide much information about speed
			 * reconfig. So turn off PHY meanwhile to be on the safe side.
			 */
			P0sctl::access_t p0sctl = read<P0sctl>();
			P0sctl::Ipm::set(p0sctl, IPM);
			P0sctl::Det::set(p0sctl, 4);
			write<P0sctl>(p0sctl);

			/* reconfigure speed */
			p0sctl = read<P0sctl>();
			P0sctl::Spd::set(p0sctl, speed);
			write<P0sctl>(p0sctl);
		}
		/* request for reset via P0SCTL */
		P0sctl::access_t p0sctl = read<P0sctl>();
		P0sctl::Ipm::set(p0sctl, IPM);
		P0sctl::Det::set(p0sctl, 1);
		write<P0sctl>(p0sctl);
		read<P0sctl>(); /* flush */

		/* wait until reset is done and end operation */
		delayer()->usleep(1000);
		unsigned trials = 10;
		for (; trials; trials--)
		{
			/*
			 * Some controllers need much time at this point.
			 * Wait at least 200 ms to avoid bad behaviour
			 * of some old PHY controllers.
			 */
			write<P0sctl::Det>(0);
			delayer()->usleep(200000);
			p0sctl = read<P0sctl>();
			if (P0sctl::Det::get(p0sctl) == 0 &&
			    P0sctl::Ipm::get(p0sctl) == 3) break;
		}
		if (!trials) {
			PERR("PORT0 resume after hard reset failed");
			return -1;
		}
		return 0;
	}

	/**
	 * Debounce link at port 0
	 *
	 * \param trials    total amount of debouncing trials
	 * \param trial_us  time to wait between two trials
	 * \param stable    targeted amount of consecutive stable trials
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 *
	 * We give the port some time in order that the P0SSTS becomes stable
	 * over multiple reads. The call is successful if the register gets
	 * stable in time and with P0SSTS.DET saying "connection established".
	 */
	int p0_debounce(unsigned const trials, unsigned const trial_us,
	                unsigned const stable)
	{
		unsigned t = 0; /* current trial */
		unsigned s = 0; /* current amount of stable trials */
		P0ssts::access_t old_det = read<P0ssts::Det>();
		for (; t < trials; t++) {
			delayer()->usleep(trial_us);
			P0ssts::access_t new_det = read<P0ssts::Det>();
			if (new_det == 3 && new_det == old_det) {
				s++;
				if (s >= stable) break;
			} else s = 0;
			old_det = new_det;
		}
		if (t >= trials) {
			printf("PORT0 failed debouncing\n");
			return -1;
		}
		return 0;
	}

	/**
	 * Disable interrupt reception for port 0
	 */
	void p0_disable_irqs() { write<P0ie>(0); }

	/**
	 * Get port 0 and its device ready for NCQ commands
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int p0_init()
	{
		/* disable command processing and FIS reception */
		p0_disable_cmd_processing();
		write<P0cmd::Fre>(0);
		if (!wait_for<P0cmd::Fr>(0, *delayer(), 500, 1000)) {
			PERR("PORT0 failed to stop FIS reception");
			return -1;
		}
		/* clear all S-errors and interrupts */
		p0_clear_errors();
		write<P0is>(read<P0is>());
		write<Is::Ips>(1);

		/* activate */
		write<Ghc::Ie>(1);
		read<Ghc>();
		P0cmd::access_t p0cmd = read<P0cmd>();
		P0cmd::Sud::set(p0cmd, 1);
		P0cmd::Pod::set(p0cmd, 1);
		P0cmd::Icc::set(p0cmd, 1);
		write<P0cmd>(p0cmd);

		/* set up command-list- and FIS-DMA */
		write<P0clb>(P0clb::Clb::masked(cl_phys));
		write<P0fb>(P0fb::Fb::masked(fb_phys));

		/* enable FIS reception and command processing */
		write<P0cmd::Fre>(1);
		read<P0cmd>();
		p0_enable_cmd_processing();

		/* disable port multiplier */
		write<P0cmd::Pma>(0);

		/* freeze AHCI */
		p0_disable_irqs();
		p0_disable_cmd_processing();

		/* clear D2H receive area */
		Fis * fis = (Fis *)(fb_virt + REG_D2H_FIS_OFFSET);
		fis->clear_d2h_rx();

		if (p0_hard_reset()) return -1;

		/* try fast debouncing without speed limit first */
		enum {
			FAST_DBC_TRIAL_US = 5000,
			SLOW_DBC_TRIAL_US = 25000,
		};
		unsigned dbc_trial_us      = FAST_DBC_TRIAL_US;
		unsigned dbc_trials        = 50;
		unsigned dbc_stable_trials = 10;
		unsigned p0_speed_limit    = 0;
		while (p0_debounce(dbc_trials, dbc_trial_us, dbc_stable_trials))
		{
			/* recover from debouncing error */
			p0_clear_errors();
			delayer()->usleep(10000);
			if (read<Is>()) {
				p0_clear_irqs();
				write<Is>(read<Is>());
			}
			if (read<P0serr>()) {
				PERR("PORT0 failed to recover from debouncing error");
				return -1;
			}

			/*
			 * FIXME
			 * Linux cleared D2H FIS again at this point but it seemed not
			 * to be necessary as all works fine without.
			 */

			/* try to lower settings for debouncing */
			if (dbc_trial_us == SLOW_DBC_TRIAL_US && p0_speed_limit == 1) {
				PERR("PORT0 debouncing failed with lowest settings");
				return -1;
			} else if (dbc_trial_us == SLOW_DBC_TRIAL_US) {
				printf("PORT0 lower link speed and retry debouncing\n");
				p0_speed_limit = 1;
				if (p0_hard_reset(1, p0_speed_limit)) return -1;
			} else {
				printf("PORT0 retry debouncing slower\n");
				dbc_trial_us = SLOW_DBC_TRIAL_US;
				if (p0_hard_reset()) return -1;
			}
		}
		p0_clear_errors();

		/* check if device is ready */
		if (!wait_for<P0tfd::Sts_bsy>(0, *delayer())) {
			PERR("PORT0 device not ready");
			return -1;
		}
		p0_enable_cmd_processing();

		if (p0_soft_reset()) return -1;
		p0_enable_irqs();
		p0_clear_errors();

		/* set ATAPI bit appropriatly */
		write<P0cmd::Atapi>(0);
		read<P0cmd>(); /* flush */

		/* check device type (LBA[31:8] = 0 means ATA device) */
		P0sig::access_t p0sig = read<P0sig>();
		if (P0sig::Lba_8_15::get(p0sig) || P0sig::Lba_16_31::get(p0sig)) {
			PERR("PORT0 driver not proved with non-ATA devices");
			return -1;
		}
		/* check device speed */
		char const * speed;
		bool speed_supported = 0;
		P0ssts::access_t p0ssts = read<P0ssts>();
		switch(P0ssts::Spd::get(p0ssts)) {
		case 1:  speed = "1.5";
				 speed_supported = 1;
				 break;
		case 2:  speed = "3";
				 break;
		case 3:  speed = "6";
				 break;
		default: speed = "?";
		}
		if (!speed_supported) {
			PERR("PORT0 driver not proved with %s Gbps", speed);
			return -1;
		}
		/* check PM state of device */
		if (P0ssts::Ipm::get(p0ssts) != 1) {
			PERR("PORT0 device not in active PM state");
			return -1;
		}
		if (VERBOSE) printf("PORT0 connected, ATA device, %s Gbps\n", speed);

		if (p0_identify_device()) return -1;
		if (p0_hides_blocks()) {
			PERR("ATA0 drive hides blocks via HPA");
			return -1;
		}

		/*
		 * FIXME
		 * At this point Linux normally reads out the parameters of the
		 * SATA DevSlp feature but the values are used only when it comes
		 * to LPM wich wasn't needed at all in our use cases. Look for
		 * 'ata_dev_configure' and 'ATA_LOG_DEVSLP_*' in Linux if you want 
		 * to add this feature.
		 */

		if (p0_transfer_mode(UDMA_133)) return -1;

		if (p0_clear_errors()) {
			PERR("ATA0 errors after initialization");
			return -1;
		}
		delayer()->usleep(10000);
		return 0;
	}

	/**
	 * Do a NCQ command and wait until it is finished
	 *
	 * \param block_nr   logical block address (LBA) of first block
	 * \param block_cnt  blocks to transfer
	 * \param phys       physical adress of receive/send buffer DMA
	 * \param w          1: write 0: read
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int ncq_command(size_t block_nr, size_t block_cnt, addr_t phys, bool w)
	{
		/* set up FIS */
		unsigned tag = 0;
		Fis * fis = (Fis *)(ct_virt + tag * CMD_TABLE_SIZE);
		fis->fpdma_queued(w, block_nr, block_cnt, tag);

		/* set up scatter/gather list */
		unsigned bytes = block_cnt * BLOCK_SIZE;
		addr_t   prd   = ct_virt + tag * CMD_TABLE_SIZE +
		                 CMD_TABLE_HEAD_SIZE;
		unsigned prdtl = 0;
		addr_t   seek  = phys;
		while (1) {
			if (bytes > BYTES_PER_PRD) {
				write_prd(prd, seek, BYTES_PER_PRD);
				seek  += BYTES_PER_PRD;
				bytes -= BYTES_PER_PRD;
				prd   += PRD_SIZE;
				prdtl++;
			} else {
				if (bytes) {
					write_prd(prd, seek, bytes);
					seek  += bytes;
					bytes -= 0;
					prdtl++;
				}
				break;
			}
			if (prdtl == 0xff) {
				PERR("Not enough PRDs available");
				return -1;
			}
		}
		/* set up command slot */
		addr_t cmd_slot  = cl_virt + tag * CMD_SLOT_SIZE;
		addr_t cmd_table = ct_phys + tag * CMD_TABLE_SIZE;
		write_cmd_slot(cmd_slot, cmd_table, true, false, 0, prdtl);

		/* issue command and wait for completion */
		write<P0sact>(1 << tag);
		write<P0ci>(1 << tag);
		irq.wait_for_irq();

		/* check command results */
		if (!read<Is::Ips>()) {
			PERR("ATA0 no IRQ raised");
			return -1;
		}
		if (p0_interpret_irqs()) return -1;
		P0sntf::access_t pmn = read<P0sntf::Pmn>();
		if (pmn) {
			write<P0sntf::Pmn>(pmn);
			PERR("ATA0 PM notify after NCQ command");
			return -1;
		}
		if (read<P0sact>()) {
			PERR("ATA0 outstanding commands after NCQ command");
			return -1;
		}
		/* end command */
		write<Is::Ips>(1);
		return 0;
	}
};

static Sata_ahci * sata_ahci() {
	static Sata_ahci sata_ahci;
	return &sata_ahci;
}

/**
 * I2C master interface
 */
struct I2c_interface : Attached_mmio
{
	enum { VERBOSE = 1 };

	enum { TX_DELAY_US = 1 };

	/********************************
	 ** MMIO structure description **
	 ********************************/

	struct Start_msg : Genode::Register<8>
	{
		struct Addr : Bitfield<1, 7> { };
	};
	struct Con : Register<0x0, 8>
	{
		struct Tx_prescaler : Bitfield<0, 4> { };
		struct Irq_pending  : Bitfield<4, 1> { };
		struct Irq_en       : Bitfield<5, 1> { };
		struct Clk_sel      : Bitfield<6, 1> { };
		struct Ack_en       : Bitfield<7, 1> { };
	};
	struct Stat : Register<0x4, 8>
	{
		struct Last_bit : Bitfield<0, 1> { };
		struct Arbitr   : Bitfield<3, 1> { };
		struct Txrx_en  : Bitfield<4, 1> { };
		struct Busy     : Bitfield<5, 1> { };
		struct Mode     : Bitfield<6, 2> { };
	};
	struct Add : Register<0x8, 8>
	{
		struct Slave_addr : Bitfield<0, 8> { };
	};
	struct Ds  : Register<0xc, 8> { };
	struct Lc  : Register<0x10, 8>
	{
		struct Sda_out_delay : Bitfield<0, 2> { };
		struct Filter_en     : Bitfield<2, 1> { };
	};

	/* single-word message that starts a multi-word message transfer */
	Start_msg::access_t const start_msg;

	/**
	 * Constructor
	 *
	 * \param base        physical MMIO base
	 * \param slave_addr  ID of the targeted slave
	 */
	I2c_interface(addr_t base, unsigned slave_addr)
	: Attached_mmio(base, 0x10000),
	  start_msg(Start_msg::Addr::bits(slave_addr))
	{ }

	/**
	 * Wether acknowledgment for last transaction can be received
	 */
	bool ack_received()
	{
		for (unsigned i = 0; i < 3; i++) {
			if (read<Con::Irq_pending>() && !read<Stat::Last_bit>()) return 1;
			delayer()->usleep(TX_DELAY_US);
		}
		PERR("I2C ack not received");
		return 0;
	}

	/**
	 * Wether arbitration errors occured during the last transaction
	 */
	bool arbitration_error()
	{
		if (read<Stat::Arbitr>()) {
			PERR("I2C arbitration failed");
			return 1;
		}
		return 0;
	}

	/**
	 * Let I2C master send a message to I2C slave
	 *
	 * \param msg       message base
	 * \param msg_size  message size
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int send(uint8_t * msg, size_t msg_size)
	{
		/* initiate message transfer */
		if (!wait_for<Stat::Busy>(0, *delayer())) {
			PERR("I2C busy");
			return -1;
		}
		Stat::access_t stat = read<Stat>();
		Stat::Txrx_en::set(stat, 1);
		Stat::Mode::set(stat, 3);
		write<Stat>(stat);
		write<Ds>(start_msg);
		delayer()->usleep(1000);
		write<Con::Tx_prescaler>(11);
		write<Stat::Busy>(1);

		/* transmit message payload */
		for (unsigned i = 0; i < msg_size; i++) {
			if (!ack_received()) return -1;
			write<Ds>(msg[i]);
			delayer()->usleep(TX_DELAY_US);
			write<Con::Irq_pending>(0);
			if (arbitration_error()) return -1;
		}
		/* end message transfer */
		if (!ack_received()) return -1;
		write<Stat::Busy>(0);
		write<Con::Irq_en>(0);
		write<Con::Irq_pending>(0); /* FIXME fixup */
		if (arbitration_error()) return -1;
		if (!wait_for<Stat::Busy>(0, *delayer())) {
			PERR("I2C end transfer failed");
			return -1;
		}
		return 0;
	}
};

/**
 * I2C control interface of SATA PHY-layer controller
 */
struct I2c_sataphy : I2c_interface
{
	enum { SLAVE_ADDR = 0x38 };

	/**
	 * Constructor
	 */
	I2c_sataphy() : I2c_interface(0x121d0000, SLAVE_ADDR) { }

	/**
	 * Enable the 40-pin interface of the SATA PHY controller
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int enable_40_pins()
	{
		/*
		 * I2C message
		 *
		 * first byte:  set address
		 * second byte: set data
		 */
		static uint8_t msg[] = { 0x3a, 0x0b };
		enum { MSG_SIZE = sizeof(msg)/sizeof(msg[0]) };

		/* send messaage */
		cmu()->top.sata_phy_i2c(1);
		if (send(msg, MSG_SIZE)) return -1;
		cmu()->top.sata_phy_i2c(0);
		if (VERBOSE) printf("SATA PHY 40-pin interface enabled\n");
		return 0;
	}

	/**
	 * Get I2C interface ready for transmissions
	 */
	void init()
	{
		cmu()->top.sata_phy_i2c(1);
		write<Add::Slave_addr>(SLAVE_ADDR);

		Con::access_t con = read<Con>();
		Con::Irq_en::set(con, 1);
		Con::Ack_en::set(con, 1);
		Con::Clk_sel::set(con, 1);
		Con::Tx_prescaler::set(con, 9);
		write<Con>(con);

		Lc::access_t lc = 0;
		Lc::Sda_out_delay::set(lc, 3);
		Lc::Filter_en::set(lc, 1);
		write<Lc>(lc);

		cmu()->top.sata_phy_i2c(0);
	}
};

static I2c_sataphy * i2c_sataphy() {
	static I2c_sataphy i2c_sataphy;
	return &i2c_sataphy;
}

/**
 * Classical control interface of SATA PHY-layer controller
 */
struct Sata_phy_ctrl : Attached_mmio
{
	enum { VERBOSE = 1 };

	/********************************
	 ** MMIO structure description **
	 ********************************/

	struct Reset : Register<0x4, 32>
	{
		struct Global   : Bitfield<1, 1> { };
		struct Non_link : Bitfield<0, 8> { };
		struct Link     : Bitfield<16, 4> { };
	};
	struct Mode0 : Register<0x10, 32>
	{
		struct P0_phy_spdmode : Bitfield<0, 2> { };
	};
	struct Ctrl0 : Register<0x14, 32>
	{
		struct P0_phy_calibrated     : Bitfield<8, 1> { };
		struct P0_phy_calibrated_sel : Bitfield<9, 1> { };
	};
	struct Phctrlm : Register<0xe0, 32>
	{
		struct High_speed : Bitfield<0, 1> { };
		struct Ref_rate   : Bitfield<1, 1> { };
	};
	struct Phstatm : Register<0xf0, 32>
	{
		struct Pll_locked : Bitfield<0, 1> { };
	};

	/**
	 * Constructor
	 */
	Sata_phy_ctrl() : Attached_mmio(0x12170000, 0x10000) { }

	/**
	 * Initialize parts of SATA PHY that are controlled classically
	 *
	 * \retval  0  call was successful
	 * \retval <0  call failed, error code
	 */
	int init()
	{
		/* reset */
		write<Reset>(0);
		write<Reset::Non_link>(~0);
		write<Reset::Link>(~0);
		write<Reset::Global>(~0);

		/* set up SATA phy generation 3 (6 Gb/s) */
		Phctrlm::access_t phctrlm = read<Phctrlm>();
		Phctrlm::Ref_rate::set(phctrlm, 0);
		Phctrlm::High_speed::set(phctrlm, 1);
		write<Phctrlm>(phctrlm);
		Ctrl0::access_t ctrl0 = read<Ctrl0>();
		Ctrl0::P0_phy_calibrated::set(ctrl0, 1);
		Ctrl0::P0_phy_calibrated_sel::set(ctrl0, 1);
		write<Ctrl0>(ctrl0);
		write<Mode0::P0_phy_spdmode>(2);
		if (i2c_sataphy()->enable_40_pins()) return -1;

		/* Release reset */
		write<Reset::Global>(0);
		write<Reset::Global>(1);

		/*
		 * FIXME Linux reads this bit once only and continues
		 *       directly, also with zero. So if we get an error
		 *       at this point we should study the Linux behavior
		 *       in more depth.
		 */
		if (!wait_for<Phstatm::Pll_locked>(1, *delayer())) {
			PERR("PLL lock failed");
			return -1;
		}
		if (VERBOSE) printf("SATA PHY initialized\n");
		return 0;
	}
};

static Sata_phy_ctrl * sata_phy_ctrl() {
	static Sata_phy_ctrl sata_phy_ctrl;
	return &sata_phy_ctrl;
}


/*****************
 ** Ahci_driver **
 *****************/

Ahci_driver::Ahci_driver()
{
	i2c_sataphy()->init();
	cmu()->top.init_sata();
	pmu()->init_sata();
	if (sata_phy_ctrl()->init()) throw Io_error();
	if (sata_ahci()->init())     throw Io_error();
	if (sata_ahci()->p0_init())  throw Io_error();
}

int Ahci_driver::_ncq_command(size_t const block_nr, size_t const block_cnt,
                              addr_t const phys, bool const w)
{
	/* sanity check */
	if (!block_cnt || (block_nr + block_cnt) > block_count()) {
		PERR("Sanity check failed on block driver command");
		return -1;
	}

	/* hardware */
	return sata_ahci()->ncq_command(block_nr, block_cnt, phys, w);
}

size_t Ahci_driver::block_count() { return sata_ahci()->block_cnt; }
size_t Ahci_driver::block_size()  { return Sata_ahci::BLOCK_SIZE; }

Ram_dataspace_capability Ahci_driver::alloc_dma_buffer(size_t size) {
	return env()->ram_session()->alloc(size, 0); }

void Ahci_driver::read(size_t, size_t, char *)
{
	PERR("Not implemented");
	throw Io_error();
}

void Ahci_driver::write(size_t, size_t, char const *)
{
	PERR("Not implemented");
	throw Io_error();
}
