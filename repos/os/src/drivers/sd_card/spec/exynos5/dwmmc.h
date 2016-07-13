/*
 * \brief  DesignWare Multimedia Card interface
 * \author Sebastian Sumpf
 * \date   2013-03-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__SD_CARD__SPEC__EXYNOS5__DWMMC_H_
#define _DRIVERS__SD_CARD__SPEC__EXYNOS5__DWMMC_H_

#include <drivers/board_base.h>
#include <irq_session/connection.h>
#include <os/attached_ram_dataspace.h>
#include <util/mmio.h>

#include <sd_card.h>

struct Dwmmc : Genode::Mmio
{
	enum { verbose = false };

	/*
	 * These apply to card controller 0 and 1 only
	 */
	enum {
		HOST_FREQ = 52000000,  /* Hz */
		CLK_FREQ  = 400000000, /* Hz */

	/* CLK_FREQ / (2 * CLK_DIV <= HOST_FREQ) */
		CLK_DIV_52Mhz  = 4,
		CLK_DIV_400Khz = 0xff,
	};

	Dwmmc(Genode::addr_t base) : Genode::Mmio(base) { }

	template <Genode::off_t OFFSET, bool STRICT_WRITE = false>
	struct Register : Genode::Mmio::Register<OFFSET, 32, STRICT_WRITE> { };

	/**
	 * Control register
	 */
	struct Ctrl : Register<0x0>
	{
		/* Controller/FIFO/DMA reset */
		struct Reset             : Bitfield<0, 3> { };
		struct Global_interrupt  : Bitfield<4, 1> { };
		struct Dma_enable        : Bitfield<5, 1> { };
		struct Use_internal_dmac : Bitfield<25, 1> { };
	};

	/**
	 * Power-enable register
	 */
	struct Pwren : Register<0x4> { };

	/**
	 * Clock-devider register
	 */
	struct Clkdiv : Register<0x8> { };

	/**
	 * Clock-enable register
	 */

	struct Clkena : Register<0x10> { };

	/**
	 * Timeout register
	 */
	struct Tmout : Register<0x14> { };

	/**
	 * Card-type register
	 */
	struct Ctype : Register<0x18, true> { };

	/**
	 * Block-size register
	 */
	struct Blksize : Register<0x1c> { };

	/**
	 * Byte-count register
	 */
	struct Bytcnt  : Register<0x20> { };
	/**
	 * Interrupt-mask register
	 */
	struct Intmask : Register<0x24> { };

	/**
	 * Command-argument register
	 */
	struct Cmdarg : Register<0x28> { };

	/**
	 * Command register
	 */
	struct Cmd : Register<0x2c>
	{
		struct Index                       : Bitfield<0,  6> { };
		struct Rsp_type                    : Bitfield<6,  3>
		{
			enum Response { RESPONSE_NONE             = 0,
			                RESPONSE_48_BIT           = 1,
			                RESPONSE_48_BIT_WITH_BUSY = 5,
			                RESPONSE_136_BIT          = 7,
			};
		};
		struct Data_expected               : Bitfield<9,  1> { };
		struct Write                       : Bitfield<10, 1> { };
		struct Wait_prvdata_complete       : Bitfield<13, 1> { };
		struct Init_sequence               : Bitfield<15, 1> { };
		struct Update_clock_registers_only : Bitfield<21, 1> { };
		struct Use_hold_reg                : Bitfield<29, 1> { };
		struct Start_cmd                   : Bitfield<31, 1> { };
	};

	/**
	 * Response bits 0..127
	 */
	struct Rsp0 : Register<0x30> { };
	struct Rsp1 : Register<0x34> { };
	struct Rsp2 : Register<0x38> { };
	struct Rsp3 : Register<0x3c> { };

	/**
	 * Interrupt-status register
	 */
	struct Mintsts : Register<0x40> { };
	struct Rintsts : Register<0x44, true>
	{
		struct Response_error     : Bitfield<1, 1> { };
		struct Data_transfer_over : Bitfield<3, 1> { };
		struct Command_done       : Bitfield<2, 1> { };
		struct Data_crc_error     : Bitfield<7, 1> { };
		struct Response_timeout   : Bitfield<8, 1> { };
		struct Data_read_timeout  : Bitfield<9, 1> { };
	};

	/**
	 * Status register
	 */
	struct Status : Register<0x48>
	{
		struct Data_busy : Bitfield<9, 1> { };
	};

	/**
	 * Fifo-threshold register
	 */
	struct Fifoth : Register<0x4c> { };

	/**
	 * Bus-mode register
	 */
	struct Bmod : Register<0x80, true>
	{
		struct Fixed_burst : Bitfield<1, 1> { };
		struct Idmac_enable : Bitfield<7, 1> { };
	};

	/**
	 * Poll demant register
	 */
	struct Pldmnd : Register<0x84> { };

	struct Idsts : Register<0x8c> { };
	struct Idinten : Register<0x90, true> { };

	/**
	 * Descriptor list base-address register
	 */
	struct Dbaddr : Register<0x88> { };

	/**
	 * Clock selector
	 */
	struct Clksel : Register<0x9c> { };

	struct Emmc_ddr_req : Register<0x10c, true> { };

	typedef Genode::size_t size_t;

	void powerup()
	{
		write<Pwren>(1);
	}

	bool reset(Delayer &delayer)
	{
		/* set all three bits */
		write<Ctrl::Reset>(0x7);

		if (!wait_for<Ctrl::Reset>(0, delayer, 100, 1000)) {
			Genode::error("Could not reset host contoller");
			return false;
		}

		return true;
	}

	void reset_fifo(Delayer &delayer)
	{
		write<Ctrl::Reset>(0x2);

		if (!wait_for<Ctrl::Reset>(0, delayer, 100, 1000))
			Genode::error("Could not reset fifo");
	}

	void disable_irq()
	{
		write<Rintsts>(~0U);
		write<Intmask>(0);
	}

	enum Bus_width {
		BUS_WIDTH_1 = 0,
		BUS_WIDTH_4 = 1,
		BUS_WIDTH_8 = 1 << 16,
	};

	void bus_width(Bus_width bus_width)
	{
		write<Ctype>(bus_width);
	}

	bool update_clock_registers(Delayer &delayer)
	{
		Cmd::access_t cmd = 0;
		Cmd::Wait_prvdata_complete::set(cmd, 1);
		Cmd::Update_clock_registers_only::set(cmd, 1);
		Cmd::Start_cmd::set(cmd, 1);

		write<Cmd>(cmd);

		if (!wait_for<Cmd::Start_cmd>(0, delayer)) {
			Genode::error("Update clock registers failed");
			return false;
		}

		return true;
	}

	bool setup_bus(unsigned clock_div, Delayer &delayer)
	{
		/* set host clock divider */
		write<Clkdiv>(clock_div);

		if (!update_clock_registers(delayer))
			return false;

		/* enable clock for card 1 */
		write<Clkena>(0x1);

		if (!update_clock_registers(delayer))
			return false;

		delayer.usleep(10 * 1000);
		return true;
	}
};


struct Exynos5_msh_controller : private Dwmmc, Sd_card::Host_controller
{
	private:

		enum { BLOCK_SIZE = 512 };

		/**
		 * DMA descriptpor
		 */
		struct Idmac_desc
		{
			enum Flags {
				NONE = 0,
				DIC  = 1 << 1,
				LD   = 1 << 2,
				FS   = 1 << 3,
				CH   = 1 << 4,
				ER   = 1 << 5,
				OWN  = 1 << 31,
			};

			unsigned  flags;
			unsigned  bytes;
			unsigned  addr;
			unsigned  next;

			size_t set(size_t block_count, Genode::addr_t phys_addr, Flags flag)
			{
				enum  { MAX_BLOCKS = 8 };
				flags  = OWN | flag | (block_count <= MAX_BLOCKS ? LD : (CH | DIC));
				bytes  = ((block_count < MAX_BLOCKS) ? block_count : MAX_BLOCKS) * BLOCK_SIZE;
				addr   = phys_addr;

				return block_count < MAX_BLOCKS ? 0 : block_count  - MAX_BLOCKS;
			}

			void dump()
			{
				Genode::log("this: ",  this,               " "
				            "flags: ", Genode::Hex(flags), " "
				            "bytes: ", bytes,              " "
				            "addr: ",  Genode::Hex(addr),  " "
				            "next:",   Genode::Hex(next));
			}
		};

		/*
		 * DMA descriptors
		 */
		enum { IDMAC_DESC_MAX_ENTRIES  = 1024 /* can be up to 65536 */};
		Genode::Attached_ram_dataspace _idmac_desc_ds;
		Idmac_desc             * const _idmac_desc;
		Genode::addr_t           const _idmac_desc_phys;

		Delayer           &_delayer;
		Sd_card::Card_info _card_info;

		Genode::Irq_connection  _irq;
		Genode::Signal_receiver _irq_rec;
		Genode::Signal_context  _irq_ctx;

		Sd_card::Card_info _init()
		{
			using namespace Sd_card;

			powerup();

			if (!reset(_delayer))
				throw Detection_failed();

			write<Emmc_ddr_req>(0x1);

			disable_irq();

			write<Tmout>(~0U);
			write<Idinten>(0);
			write<Bmod>(1);
			write<Bytcnt>(0);

			write<Fifoth>(0x203f0040);

			/* set to one bit transfer Bit */
			if (!setup_bus(CLK_DIV_400Khz, _delayer))
				throw Detection_failed();

			bus_width(BUS_WIDTH_1);

			if (!issue_command(Go_idle_state())) {
				Genode::warning("Go_idle_state command failed");
				throw Detection_failed();
			}

			_delayer.usleep(2000);

			if (!issue_command(Send_if_cond())) {
				Genode::warning("Send_if_cond command failed");
				throw Detection_failed();
			}

			 /* if this succeeds it is an SD card */
			if ((read<Rsp0>() & 0xff) == 0xaa)
				Genode::log("Found SD card");

			/*
			 * We need to issue the same Mmc_send_op_cond command multiple
			 * times. The first time, we receive the status information. On
			 * subsequent attempts, the response tells us that the card is
			 * busy. Usually, the command is issued twice. We give up if the
			 * card is not reaching busy state after one second.
			 */
			unsigned i = 1000;
			unsigned voltages = 0x300080;
			unsigned arg = 0;
			for (; i > 0; --i) {
				if (!issue_command(Mmc_send_op_cond(arg, true))) {
					Genode::warning("Sd_send_op_cond command failed");
					throw Detection_failed();
				}

				arg = read<Rsp0>();
				arg = (voltages & (arg & 0x007FFF80)) | (arg & 0x60000000);

				_delayer.usleep(1000);

				if (Ocr::Busy::get(read<Rsp0>()))
					break;
			}

			if (i == 0) {
				Genode::error("Send_op_cond timed out, could no power-on SD/MMC card");
				throw Detection_failed();
			}

			Card_info card_info = _detect_mmc();

			/* switch frequency to high speed */
			enum { EXT_CSD_HS_TIMING = 185 };
			if (!issue_command(Mmc_switch(EXT_CSD_HS_TIMING, 1))) {
				Genode::error("Error setting high speed frequency");
				throw Detection_failed();
			}

			enum { EXT_CSD_BUS_WIDTH = 183 };
			/* set card to 8 bit */
			if (!issue_command(Mmc_switch(EXT_CSD_BUS_WIDTH, 2))) {
				Genode::error("Error setting card bus width");
				throw Detection_failed();
			}

			bus_width(BUS_WIDTH_8);

			/* set to eight bit transfer Bit */
			if (!setup_bus(CLK_DIV_52Mhz, _delayer)) {
				Genode::error("Error setting bus to high speed");
				throw Detection_failed();
			}

			/*
			 * Enable Interrupts data read timeout | data transfer done | response
			 * error
			 */
			write<Intmask>(0x28a);
			write<Ctrl::Global_interrupt>(1);
			return card_info;
		}

		bool _setup_idmac_descriptor_table(size_t block_count,
		                                   Genode::addr_t phys_addr)
		{
			size_t const max_idmac_block_count = IDMAC_DESC_MAX_ENTRIES * 8;

			if (block_count > max_idmac_block_count) {
				Genode::error("Block request too large");
				return false;
			}

			reset_fifo(_delayer);

			Idmac_desc::Flags flags = Idmac_desc::FS;
			size_t b = block_count;
			int index = 0;
			for (index = 0; b;
			     index++, phys_addr += 0x1000, flags = Idmac_desc::NONE) {
				b = _idmac_desc[index].set(b, phys_addr, flags);
				_idmac_desc[index].next = _idmac_desc_phys + ((index + 1) * sizeof(Idmac_desc));
			}

			_idmac_desc[index].next = (unsigned)_idmac_desc;
			_idmac_desc[index].flags |= Idmac_desc::ER;
			write<Dbaddr>(_idmac_desc_phys);

			write<Ctrl::Dma_enable>(1);
			write<Ctrl::Use_internal_dmac>(1);

			write<Bmod::Fixed_burst>(1);
			write<Bmod::Idmac_enable>(1);

			write<Blksize>(BLOCK_SIZE);
			write<Bytcnt>(BLOCK_SIZE * block_count);

			write<Pldmnd>(1);

			return true;
		}

		void _wait_for_irq()
		{
			/*
			 * Acknowledge the IRQ first to implicitly activate
			 * receiving of further IRQ signals on the first usage
			 * of this method.
			 */
			_irq.ack_irq();
			_irq_rec.wait_for_signal();
		}

		bool _wait_for_transfer_complete()
		{
			while (1) {
				_wait_for_irq();

				if (read<Rintsts::Data_transfer_over>()) {
					write<Rintsts>(~0U);
					return true;
				}

				if (read<Rintsts::Response_error>()) {
					Genode::error("Response error");
					return false;
				}

				if (read<Rintsts::Data_read_timeout>()) {
					Genode::error("Data read timeout");
					return false;
				}

				if (read<Rintsts::Data_crc_error>()) {
					Genode::error("CRC error");
					return false;
				}
			}
		}


	public:

		enum { IRQ_NUMBER = Genode::Board_base::SDMMC0_IRQ };

		Exynos5_msh_controller(Server::Entrypoint &ep,
		                       Genode::addr_t const mmio_base, Delayer &delayer,
		                       bool use_dma)
		: Dwmmc(mmio_base),
			_idmac_desc_ds(Genode::env()->ram_session(),
			               IDMAC_DESC_MAX_ENTRIES*sizeof(Idmac_desc),
			               Genode::UNCACHED),
			_idmac_desc(_idmac_desc_ds.local_addr<Idmac_desc>()),
			_idmac_desc_phys(Genode::Dataspace_client(_idmac_desc_ds.cap()).phys_addr()),
			_delayer(delayer), _card_info(_init()), _irq(IRQ_NUMBER)
		{
			_irq.sigh(_irq_rec.manage(&_irq_ctx));
		}

		~Exynos5_msh_controller() { _irq_rec.dissolve(&_irq_ctx); }

		bool _issue_command(Sd_card::Command_base const &command)
		{
			if (verbose)
				Genode::log("-> ", command);

			if (!wait_for<Status::Data_busy>(0, _delayer, 10000, 100)) {
				Genode::error("wait for State::Data_busy timed out ",
				              Genode::Hex(read<Status>()));
				return false;
			}

			write<Rintsts>(~0UL);

			/* write command argument */
			write<Cmdarg>(command.arg);

			Cmd::access_t cmd = 0;
			Cmd::Index::set(cmd, command.index);

			if (command.transfer != Sd_card::TRANSFER_NONE) {
				/* set data-direction bit depending on the command */
				bool const write = command.transfer == Sd_card::TRANSFER_WRITE;
				Cmd::Data_expected::set(cmd, 1);
				Cmd::Write::set(cmd, write ? 1 : 0);
			}

			Cmd::access_t rsp_type = 0;
			switch (command.rsp_type) {
			case Sd_card::RESPONSE_NONE:             rsp_type = Cmd::Rsp_type::RESPONSE_NONE;             break;
			case Sd_card::RESPONSE_136_BIT:          rsp_type = Cmd::Rsp_type::RESPONSE_136_BIT;          break;
			case Sd_card::RESPONSE_48_BIT:           rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT;           break;
			case Sd_card::RESPONSE_48_BIT_WITH_BUSY: rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT_WITH_BUSY; break;
			}

			Cmd::Rsp_type::set(cmd, rsp_type);
			Cmd::Start_cmd::set(cmd, 1);
			Cmd::Use_hold_reg::set(cmd ,1);
			Cmd::Wait_prvdata_complete::set(cmd, 1);

			if (command.index == 0)
				Cmd::Init_sequence::set(cmd, 1);

			/* issue command */
			write<Cmd>(cmd);

			if (!wait_for<Rintsts::Command_done>(1, _delayer, 10000, 100)) {
				Genode::error("command failed "
				              "Rintst: ", read<Rintsts>(), " "
				              "Mintst: ", read<Mintsts>(), " "
				              "Status: ", read<Status>());

				if (read<Rintsts::Response_timeout>())
					Genode::warning("timeout");

				if (read<Rintsts::Response_error>())
					Genode::warning("repsonse error");

				return false;
			}

			/* acknowledge interrupt */
			write<Rintsts::Command_done>(1);

			_delayer.usleep(100);

			return true;
		}

		Sd_card::Cid _read_cid()
		{
			Sd_card::Cid cid;
			cid.raw_0 = read<Rsp0>();
			cid.raw_1 = read<Rsp1>();
			cid.raw_2 = read<Rsp2>();
			cid.raw_3 = read<Rsp3>();
			return cid;
		}

		Sd_card::Csd _read_csd()
		{
			Sd_card::Csd csd;
			csd.csd0 = read<Rsp0>();
			csd.csd1 = read<Rsp1>();
			csd.csd2 = read<Rsp2>();
			csd.csd3 = read<Rsp3>();
			return csd;
		}

		unsigned _read_rca() { return 0; }

		size_t _read_ext_csd()
		{
			using namespace Genode;
			Attached_ram_dataspace ds(env()->ram_session(), 0x1000, UNCACHED);

			addr_t phys = Genode::Dataspace_client(ds.cap()).phys_addr();
			_setup_idmac_descriptor_table(1, phys);

			if (!issue_command(Sd_card::Mmc_send_ext_csd()))
				throw Detection_failed();

			if (!wait_for<Rintsts::Data_transfer_over>(1, _delayer)) {
				Genode::error("cannot retrieve extented CSD");
				throw Detection_failed();
			}
			/* clear IRQ */
			write<Rintsts::Data_transfer_over>(1);

			/* contruct extented CSD */
			Sd_card::Ext_csd csd((addr_t)ds.local_addr<addr_t>());

			/* read revision */
			if (csd.read<Sd_card::Ext_csd::Revision>() < 2) {
				Genode::error("extented CSD revision is < 2");
				throw Detection_failed();
			}

			/* return sector count */
			uint64_t capacity =  csd.read<Sd_card::Ext_csd::Sector_count>() * BLOCK_SIZE;

			/* to MB */
			return capacity / (1024 * 1024);
		}

		Sd_card::Card_info card_info() const
		{
			return _card_info;
		}

		bool read_blocks_dma(Block::sector_t block_number, size_t block_count,
		                     Genode::addr_t buffer_phys)
		{
			if (!_setup_idmac_descriptor_table(block_count, buffer_phys))
				return false;

			if (!_issue_command(Sd_card::Read_multiple_block(block_number))) {
				Genode::error("Read_multiple_block failed, Status: ",
				              Genode::Hex(read<Status>()));
				return false;
			}

			bool complete = _wait_for_transfer_complete();

			if (!_issue_command(Sd_card::Stop_transmission())) {
				Genode::error("unable to stop transmission");
				return false;
			}

			return complete;
		}

		bool write_blocks_dma(Block::sector_t block_number, size_t block_count,
		                      Genode::addr_t buffer_phys)
		{
			if (!_setup_idmac_descriptor_table(block_count, buffer_phys))
				return false;

			if (!_issue_command(Sd_card::Write_multiple_block(block_number))) {
				Genode::error("Read_multiple_block failed, Status: ",
				              Genode::Hex(read<Status>()));
				return false;
			}

			bool complete = _wait_for_transfer_complete();

			if (!_issue_command(Sd_card::Stop_transmission())) {
				Genode::error("unable to stop transmission");
				return false;
			}

			return complete;
		}
};

#endif /* _DRIVERS__SD_CARD__SPEC__EXYNOS5__DWMMC_H_ */
