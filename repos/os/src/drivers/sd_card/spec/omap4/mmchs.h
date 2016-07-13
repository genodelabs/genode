/*
 * \brief  OMAP4 MMCHS controller registers
 * \author Norman Feske
 * \date   2012-07-03
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__SD_CARD__SPEC__OMAP4__MMCHS_H_
#define _DRIVERS__SD_CARD__SPEC__OMAP4__MMCHS_H_

/* Genode includes */
#include <util/mmio.h>
#include <os/attached_ram_dataspace.h>
#include <irq_session/connection.h>
#include <drivers/board_base.h>

/* local includes */
#include <sd_card.h>

struct Mmchs : Genode::Mmio
{
	enum { verbose = false };

	typedef Genode::size_t size_t;

	Mmchs(Genode::addr_t const mmio_base) : Genode::Mmio(mmio_base) { }

	struct Sysconfig : Register<0x110, 32>
	{
		struct Autoidle  : Bitfield<0, 1> { };
		struct Softreset : Bitfield<1, 1> { };
		struct Sidlemode : Bitfield<3, 2>
		{
			enum { NO_IDLE = 1 };
		};
		struct Clockactivity : Bitfield<8, 2>
		{
			enum { BOTH_ACTIVE = 3 };
		};
	};

	struct Sysstatus : Register<0x114, 32>
	{
		struct Reset_done : Bitfield<0, 1> { };
	};

	struct Con : Register<0x12c, 32>
	{
		/**
		 * Open drain mode
		 */
		struct Od : Bitfield<0, 1> { };

		/**
		 * Start initialization stream
		 */
		struct Init : Bitfield<1, 1> { };

		struct Dw8 : Bitfield<5, 1> { };

		/**
		 * Master master slave selection (set if master DMA)
		 */
		struct Dma_mns : Bitfield<20, 1> { };

	};

	/**
	 * Command register
	 */
	struct Cmd : Register<0x20c, 32>
	{
		struct Index : Bitfield<24, 6> { };

		/**
		 * Data present
		 */
		struct Dp : Bitfield<21, 1> { };

		struct Rsp_type : Bitfield<16, 2>
		{
			enum Response { RESPONSE_NONE             = 0,
			                RESPONSE_136_BIT          = 1,
			                RESPONSE_48_BIT           = 2,
			                RESPONSE_48_BIT_WITH_BUSY = 3 };
		};

		/**
		 * Data direction
		 */
		struct Ddir : Bitfield<4, 1>
		{
			enum { WRITE = 0, READ = 1 };
		};

		/**
		 * Block count enable
		 */
		struct Bce : Bitfield<1, 1> { };

		/**
		 * Multiple block select
		 */
		struct Msbs : Bitfield<5, 1> { };

		/**
		 * Auto-CMD12 enable
		 */
		struct Acen : Bitfield<2, 1> { };

		/**
		 * DMA enable
		 */
		struct De : Bitfield<0, 1> { };
	};

	/**
	 * Transfer length configuration
	 */
	struct Blk : Register<0x204, 32>
	{
		struct Blen : Bitfield<0, 12> { };
		struct Nblk : Bitfield<16, 16> { };
	};

	/**
	 * Command argument
	 */
	struct Arg : Register<0x208, 32> { };

	/**
	 * Response bits 0..31
	 */
	struct Rsp10 : Register<0x210, 32> { };

	/**
	 * Response bits 32..63
	 */
	struct Rsp32 : Register<0x214, 32> { };

	/**
	 * Response bits 64..95
	 */
	struct Rsp54 : Register<0x218, 32> { };

	/**
	 * Response bits 96..127
	 */
	struct Rsp76 : Register<0x21c, 32> { };

	struct Data : Register<0x220, 32> { };

	struct Pstate : Register<0x224, 32>
	{
		/**
		 * Command inhibit
		 */
		struct Cmdi : Bitfield<0, 1> { };

		/**
		 * Buffer write enable status
		 */
		struct Bwe : Bitfield<10, 1> { };

		/**
		 * Buffer read enable status
		 */
		struct Bre : Bitfield<11, 1> { };
	};

	struct Hctl : Register<0x228, 32>
	{
		/**
		 * Wakeup event enable
		 */
		struct Iwe : Bitfield<24, 1> { };

		/**
		 * SD bus power
		 */
		struct Sdbp : Bitfield<8, 1>
		{
			enum { POWER_OFF = 0, POWER_ON = 1 };
		};

		/**
		 * SD voltage select
		 */
		struct Sdvs : Bitfield<9, 3>
		{
			enum Voltage { VOLTAGE_1_8 = 5,
			               VOLTAGE_3_0 = 6,
			               VOLTAGE_3_3 = 7 };
		};

		/**
		 * Data transfer width
		 */
		struct Dtw : Bitfield<1, 1>
		{
			enum { ONE_BIT = 0, FOUR_BITS = 1 };
		};
	};

	struct Sysctl : Register<0x22c, 32>
	{
		/**
		 * Internal clock enable
		 */
		struct Ice : Bitfield<0, 1> { };

		/**
		 * Internal clock stable
		 */
		struct Ics : Bitfield<1, 1> { };

		/**
		 * Clock enable
		 */
		struct Ce : Bitfield<2, 1> { };

		/**
		 * Clock divider
		 */
		struct Clkd : Bitfield<6, 10> { };

		/**
		 * Software reset all
		 */
		struct Sra : Bitfield<24, 1> { };

		/**
		 * Software reset for command line
		 */
		struct Src : Bitfield<25, 1> { };

		/**
		 * Data timeout counter
		 */
		struct Dto : Bitfield<16, 4>
		{
			enum { TCF_2_POW_27 = 0xe };
		};
	};

	struct Stat : Register<0x230, 32>
	{
		/**
		 * Transfer completed
		 */
		struct Tc : Bitfield<1, 1> { };

		/**
		 * Command completed
		 */
		struct Cc : Bitfield<0, 1> { };

		/**
		 * Error
		 */
		struct Erri : Bitfield<15, 1> { };

		/**
		 * Command timed out
		 */
		struct Cto  : Bitfield<16, 1> { };
	};

	/**
	 * Interrupt enable
	 */
	struct Ie : Register<0x234, 32>
	{
		/**
		 * Command completed
		 */
		struct Cc_enable : Bitfield<0, 1> { };

		/**
		 * Transfer completed
		 */
		struct Tc_enable : Bitfield<1, 1> { };

		/**
		 * Card interrupt
		 */
		struct Cirq_enable : Bitfield<8, 1> { };

		/**
		 * Command timeout error
		 */
		struct Cto_enable : Bitfield<16, 1> { };
	};

	struct Ise : Register<0x238, 32>
	{
		/*
		 * The naming follows the lines of the 'Ie' register
		 */
		struct Tc_sigen  : Bitfield<1, 1> { };
		struct Cto_sigen : Bitfield<16, 1> { };
	};

	/**
	 * Capabilities
	 */
	struct Capa : Register<0x240, 32>
	{
		struct Vs30 : Bitfield<25, 1> { };
		struct Vs18 : Bitfield<26, 1> { };
	};

	/**
	 * ADMA system address
	 *
	 * Base address of the ADMA descriptor table
	 */
	struct Admasal : Register<0x258, 32> { };

	/**
	 * ADMA descriptor layout
	 */
	struct Adma_desc : Genode::Register<64>
	{
		struct Valid   : Bitfield<0, 1> { };
		struct Ent     : Bitfield<1, 1> { };
		struct Int     : Bitfield<2, 1> { };
		struct Act1    : Bitfield<4, 1> { };
		struct Act2    : Bitfield<5, 1> { };
		struct Length  : Bitfield<16, 16> { };
		struct Address : Bitfield<32, 32> { };
	};

	bool reset_cmd_line(Delayer &delayer)
	{
		write<Sysctl::Src>(1);

		/*
		 * We must poll quickly. If we waited too long until checking the bit,
		 * the polling would be infinite. Apparently the hardware depends on
		 * the timing here.
		 */
		if (!wait_for<Sysctl::Src>(1, delayer, 1000, 0)) {
			Genode::error("reset of cmd line timed out (src != 1)");
			return false;
		}

		if (!wait_for<Sysctl::Src>(0, delayer, 1000, 0)) {
			Genode::error("reset of cmd line timed out (src != 0)");
			return false;
		}

		return true;
	}

	/* XXX unused */
	bool soft_reset_all(Delayer &delayer)
	{
		write<Sysctl::Sra>(1);

		if (!wait_for<Sysctl::Sra>(1, delayer, 1000, 0)) {
			Genode::error("soft reset all timed out (src != 1)");
			return false;
		}

		return true;
	}

	void disable_irq()
	{
		write<Ise>(0);
		write<Ie>(0);
		write<Stat>(~0);
	}

	enum Bus_width { BUS_WIDTH_1, BUS_WIDTH_4 };

	void bus_width(Bus_width bus_width)
	{
		switch (bus_width) {
		case BUS_WIDTH_1:
			write<Con::Dw8>(0);
			write<Hctl::Dtw>(Hctl::Dtw::ONE_BIT);
			break;

		case BUS_WIDTH_4:
			write<Con::Dw8>(0);
			write<Hctl::Dtw>(Hctl::Dtw::FOUR_BITS);
			break;
		}
	}

	bool sd_bus_power_on(Delayer &delayer)
	{
		write<Hctl::Sdbp>(Hctl::Sdbp::POWER_ON);

		if (!wait_for<Hctl::Sdbp>(1, delayer)) {
			Genode::error("setting Hctl::Sdbp timed out");
			return false;
		}
		return true;
	}

	void stop_clock()
	{
		write<Sysctl::Ce>(0);
	}

	enum Clock_divider { CLOCK_DIV_0, CLOCK_DIV_240 };

	bool set_and_enable_clock(enum Clock_divider divider, Delayer &delayer)
	{
		write<Sysctl::Dto>(Sysctl::Dto::TCF_2_POW_27);

		switch (divider) {
		case CLOCK_DIV_0:   write<Sysctl::Clkd>(0);   break;
		case CLOCK_DIV_240: write<Sysctl::Clkd>(240); break;
		}

		write<Sysctl::Ice>(1);

		/* wait for clock to become stable */
		if (!wait_for<Sysctl::Ics>(1, delayer)) {
			Genode::error("clock enable timed out");
			return false;
		}

		/* enable clock */
		write<Sysctl::Ce>(1);

		return true;
	}

	enum Voltage { VOLTAGE_3_0, VOLTAGE_1_8 };

	void set_bus_power(Voltage voltage)
	{
		switch (voltage) {
		case VOLTAGE_3_0:
			write<Hctl::Sdvs>(Hctl::Sdvs::VOLTAGE_3_0);
			break;

		case VOLTAGE_1_8:
			write<Hctl::Sdvs>(Hctl::Sdvs::VOLTAGE_1_8);
			break;
		}

		write<Capa::Vs18>(1);

		if (voltage == VOLTAGE_3_0)
			write<Capa::Vs30>(1);
	}

	bool init_stream(Delayer &delayer)
	{
		write<Ie>(0x307f0033);

		/* start initialization sequence */
		write<Con::Init>(1);

		write<Cmd>(0);

		if (!wait_for<Stat::Cc>(1, delayer, 1000*1000, 0)) {
			Genode::error("init stream timed out");
			return false;
		}

		/* stop initialization sequence */
		write<Con::Init>(0);

		write<Stat>(~0);
		read<Stat>();

		return true;
	}

	struct Hl_sysconfig : Register<0x10, 32> { };
};


struct Omap4_hsmmc_controller : private Mmchs, public Sd_card::Host_controller
{
	private:

		Delayer           &_delayer;
		Sd_card::Card_info _card_info;
		bool const         _use_dma;

		/*
		 * DMA memory for holding the ADMA descriptor table
		 */
		enum { ADMA_DESC_MAX_ENTRIES = 1024 };
		Genode::Attached_ram_dataspace _adma_desc_ds;
		Adma_desc::access_t *    const _adma_desc;
		Genode::addr_t           const _adma_desc_phys;

		Genode::Irq_connection  _irq;
		Genode::Signal_receiver _irq_rec;
		Genode::Signal_context  _irq_ctx;

		Sd_card::Card_info _init()
		{
			using namespace Sd_card;

			write<Sysconfig>(0x2015);
			write<Hctl>(0x0);

			set_bus_power(VOLTAGE_3_0);

			if (!sd_bus_power_on(_delayer)) {
				Genode::error("sd_bus_power failed");
			}

			disable_irq();

			bus_width(BUS_WIDTH_1);

			_delayer.usleep(10*1000);

			stop_clock();
			if (!set_and_enable_clock(CLOCK_DIV_240, _delayer)) {
				Genode::error("set_clock failed");
				throw Detection_failed();
			}

			if (!init_stream(_delayer)) {
				Genode::error("sending the initialization stream failed");
				throw Detection_failed();
			}

			write<Blk>(0);

			_delayer.usleep(1000);

			if (!issue_command(Go_idle_state())) {
				Genode::error("Go_idle_state command failed");
				throw Detection_failed();
			}

			_delayer.usleep(2000);

			if (!issue_command(Send_if_cond())) {
				Genode::error("Send_if_cond command failed");
				throw Detection_failed();
			}

			if (read<Rsp10>() != 0x1aa) {
				Genode::error("unexpected response of Send_if_cond command");
				throw Detection_failed();
			}

			/*
			 * We need to issue the same Sd_send_op_cond command multiple
			 * times. The first time, we receive the status information. On
			 * subsequent attempts, the response tells us that the card is
			 * busy. Usually, the command is issued twice. We give up if the
			 * card is not reaching busy state after one second.
			 */

			int i = 1000;
			for (; i > 0; --i) {
				if (!issue_command(Sd_send_op_cond(0x18000, true))) {
					Genode::warning("Sd_send_op_cond command failed");
					throw Detection_failed();
				}

				if (Ocr::Busy::get(read<Rsp10>()))
					break;

				_delayer.usleep(1000);
			}

			if (i == 0) {
				Genode::error("Sd_send_op_cond timed out, could no power-on SD card");
				throw Detection_failed();
			}

			Card_info card_info = _detect();

			/*
			 * Switch card to use 4 data signals
			 */
			if (!issue_command(Set_bus_width(Set_bus_width::Arg::Bus_width::FOUR_BITS),
			                   card_info.rca())) {
				Genode::warning("Set_bus_width(FOUR_BITS) command failed");
				throw Detection_failed();
			}

			bus_width(BUS_WIDTH_4);

			_delayer.usleep(10*1000);

			stop_clock();
			if (!set_and_enable_clock(CLOCK_DIV_0, _delayer)) {
				Genode::error("set_clock failed");
				throw Detection_failed();
			}

			/* enable master DMA */
			write<Con::Dma_mns>(1);

			/* enable IRQs */
			write<Ie::Tc_enable>(1);
			write<Ie::Cto_enable>(1);
			write<Ise::Tc_sigen>(1);
			write<Ise::Cto_sigen>(1);

			return card_info;
		}

		/**
		 * Marshal ADMA descriptors according to block request
		 *
		 * \return false if block request is too large
		 */
		bool _setup_adma_descriptor_table(size_t block_count,
		                                  Genode::addr_t out_buffer_phys)
		{
			using namespace Sd_card;

			/* reset ADMA offset to first descriptor */
			write<Admasal>(_adma_desc_phys);

			enum { BLOCK_SIZE = 512  /* bytes */ };

			size_t const max_adma_request_size = 64*1024 - 4; /* bytes */

			/*
			 * sanity check
			 *
			 * XXX An alternative to this sanity check would be to expose
			 *     the maximum DMA transfer size to the driver and let the
			 *     driver partition large requests into ones that are
			 *     supported by the controller.
			 */
			if (block_count*BLOCK_SIZE > max_adma_request_size*ADMA_DESC_MAX_ENTRIES) {
				Genode::error("Block request too large");
				return false;
			}

			/*
			 * Each ADMA descriptor can transfer up to MAX_ADMA_REQUEST_SIZE
			 * bytes. If the request is larger, we generate a list of ADMA
			 * descriptors.
			 */

			size_t const total_bytes = block_count*BLOCK_SIZE;

			/* number of bytes for which descriptors have been created */
			addr_t consumed_bytes = 0;

			for (int index = 0; consumed_bytes < total_bytes; index++) {

				size_t const remaining_bytes = total_bytes - consumed_bytes;

				/* clamp current request to maximum ADMA request size */
				size_t const curr_bytes = Genode::min(max_adma_request_size,
				                                      remaining_bytes);
				/*
				 * Assemble new ADMA descriptor
				 */
				Adma_desc::access_t desc = 0;
				Adma_desc::Address::set(desc, out_buffer_phys + consumed_bytes);
				Adma_desc::Length::set(desc, curr_bytes);

				/* set action to transfer */
				Adma_desc::Act1::set(desc, 0);
				Adma_desc::Act2::set(desc, 1);

				Adma_desc::Valid::set(desc, 1);

				/*
				 * Let the last descriptor generate transfer-complete interrupt
				 */
				if (consumed_bytes + curr_bytes == total_bytes)
					Adma_desc::Ent::set(desc, 1);

				/* install descriptor into ADMA descriptor table */
				_adma_desc[index] = desc;

				consumed_bytes += curr_bytes;
			}

			return true;
		}

		bool _wait_for_transfer_complete()
		{
			if (!wait_for<Stat::Tc>(1, _delayer, 1000*1000, 0)
			 && !wait_for<Stat::Tc>(1, _delayer)) {
				Genode::error("Stat::Tc timed out");
				return false;
			}

			/* clear transfer-completed bit */
			write<Stat::Tc>(1);
			return true;
		}

		bool _wait_for_bre()
		{
			if (!wait_for<Pstate::Bre>(1, _delayer, 1000*1000, 0)
			 && !wait_for<Pstate::Bre>(1, _delayer)) {
				Genode::error("Pstate::Bre timed out");
				return false;
			}
			return true;
		}

		bool _wait_for_bwe()
		{
			if (!wait_for<Pstate::Bwe>(1, _delayer, 1000*1000, 0)
			 && !wait_for<Pstate::Bwe>(1, _delayer)) {
				Genode::error("Pstate::Bwe timed out");
				return false;
			}
			return true;
		}

		bool _wait_for_transfer_complete_irq()
		{
			/*
			 * XXX For now, the driver works fully synchronous. We merely use
			 *     the interrupt mechanism to yield CPU time to concurrently
			 *     running processes.
			 */
			for (;;) {
				/*
				 * We ack the IRQ first to implicitly active receiving
				 * IRQ signals when entering this loop for the first time.
				 */
				_irq.ack_irq();
				_irq_rec.wait_for_signal();

				/* check for transfer completion */
				if (read<Stat::Tc>() == 1) {

					/* clear transfer-completed bit */
					write<Stat::Tc>(1);

					if (read<Stat>() != 0)
						Genode::warning("unexpected state ("
						                "Stat: ", Genode::Hex(read<Stat>()), " "
						                "Blen: ", Genode::Hex(read<Blk::Blen>()), " "
						                "Nblk: ", read<Blk::Nblk>());

					return true;
				}

				Genode::warning("unexpected interrupt, Stat: ", Genode::Hex(read<Stat>()));
			}
		}

	public:

		enum { IRQ_NUMBER = Genode::Board_base::HSMMC_IRQ };

		/**
		 * Constructor
		 *
		 * \param mmio_base  local base address of MMIO registers
		 */
		Omap4_hsmmc_controller(Genode::addr_t const mmio_base, Delayer &delayer,
		                       bool use_dma)
		:
			Mmchs(mmio_base), _delayer(delayer), _card_info(_init()),
			_use_dma(use_dma),
			_adma_desc_ds(Genode::env()->ram_session(),
			              ADMA_DESC_MAX_ENTRIES*sizeof(Adma_desc::access_t),
			              Genode::UNCACHED),
			_adma_desc(_adma_desc_ds.local_addr<Adma_desc::access_t>()),
			_adma_desc_phys(Genode::Dataspace_client(_adma_desc_ds.cap()).phys_addr()),
			_irq(IRQ_NUMBER)
		{
			_irq.sigh(_irq_rec.manage(&_irq_ctx));
		}

		~Omap4_hsmmc_controller() { _irq_rec.dissolve(&_irq_ctx); }


		/****************************************
		 ** Sd_card::Host_controller interface **
		 ****************************************/

		bool _issue_command(Sd_card::Command_base const &command)
		{
			if (verbose)
				Genode::log("-> ", command);

			if (!wait_for<Pstate::Cmdi>(0, _delayer)) {
				Genode::error("wait for Pstate::Cmdi timed out");
				return false;
			}

			/* write command argument */
			write<Arg>(command.arg);

			/* assemble command register */
			Cmd::access_t cmd = 0;
			Cmd::Index::set(cmd, command.index);
			if (command.transfer != Sd_card::TRANSFER_NONE) {

				Cmd::Dp::set(cmd);
				Cmd::Bce::set(cmd);
				Cmd::Msbs::set(cmd);

				if (command.index == Sd_card::Read_multiple_block::INDEX
				 || command.index == Sd_card::Write_multiple_block::INDEX) {
					Cmd::Acen::set(cmd);

					if (_use_dma)
						Cmd::De::set(cmd);
				}

				/* set data-direction bit depending on the command */
				bool const read = command.transfer == Sd_card::TRANSFER_READ;
				Cmd::Ddir::set(cmd, read ? Cmd::Ddir::READ : Cmd::Ddir::WRITE);
			}

			Cmd::access_t rsp_type = 0;
			switch (command.rsp_type) {
			case Sd_card::RESPONSE_NONE:             rsp_type = Cmd::Rsp_type::RESPONSE_NONE;             break;
			case Sd_card::RESPONSE_136_BIT:          rsp_type = Cmd::Rsp_type::RESPONSE_136_BIT;          break;
			case Sd_card::RESPONSE_48_BIT:           rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT;           break;
			case Sd_card::RESPONSE_48_BIT_WITH_BUSY: rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT_WITH_BUSY; break;
			}
			Cmd::Rsp_type::set(cmd, rsp_type);

			/* write command */
			write<Cmd>(cmd);

			bool result = false;

			/* wait until command is completed, return false on timeout */
			for (unsigned long i = 0; i < 1000*1000; i++) {
				Stat::access_t const stat = read<Stat>();
				if (Stat::Erri::get(stat)) {
					Genode::warning("SD command error");
					if (Stat::Cto::get(stat))
						Genode::warning("timeout");

					reset_cmd_line(_delayer);
					write<Stat::Cc>(~0);
					read<Stat>();
					result = false;
					break;
				}
				if (Stat::Cc::get(stat) == 1) {
					result = true;
					break;
				}
			}

			if (verbose)
				Genode::log("<- ", result ? "succeeded" : "timed out");

			/* clear status of command-completed bit */
			write<Stat::Cc>(1);
			read<Stat>();

			return result;
		}

		Sd_card::Card_info card_info() const
		{
			return _card_info;
		}

		Sd_card::Cid _read_cid()
		{
			Sd_card::Cid cid;
			cid.raw_0 = read<Rsp10>();
			cid.raw_1 = read<Rsp32>();
			cid.raw_2 = read<Rsp54>();
			cid.raw_3 = read<Rsp76>();
			return cid;
		}

		Sd_card::Csd _read_csd()
		{
			Sd_card::Csd csd;
			csd.csd0 = read<Rsp10>();
			csd.csd1 = read<Rsp32>();
			csd.csd2 = read<Rsp54>();
			csd.csd3 = read<Rsp76>();
			return csd;
		}

		unsigned _read_rca()
		{
			return Sd_card::Send_relative_addr::Response::Rca::get(read<Rsp10>());
		}

		/**
		 * Read data blocks from SD card
		 *
		 * \return true on success
		 */
		bool read_blocks(size_t block_number, size_t block_count, char *out_buffer)
		{
			using namespace Sd_card;

			write<Blk::Blen>(0x200);
			write<Blk::Nblk>(block_count);

			if (!issue_command(Read_multiple_block(block_number))) {
				Genode::error("Read_multiple_block failed, Stat: ",
				              Genode::Hex(read<Stat>()));
				return false;
			}

			size_t const num_accesses = block_count*512/sizeof(Data::access_t);
			Data::access_t *dst = (Data::access_t *)(out_buffer);

			for (size_t i = 0; i < num_accesses; i++) {
				if (!_wait_for_bre())
					return false;

				*dst++ = read<Data>();
			}

			return _wait_for_transfer_complete();
		}

		/**
		 * Write data blocks to SD card
		 *
		 * \return true on success
		 */
		bool write_blocks(size_t block_number, size_t block_count, char const *buffer)
		{
			using namespace Sd_card;

			write<Blk::Blen>(0x200);
			write<Blk::Nblk>(block_count);

			if (!issue_command(Write_multiple_block(block_number))) {
				Genode::error("Write_multiple_block failed");
				return false;
			}

			size_t const num_accesses = block_count*512/sizeof(Data::access_t);
			Data::access_t const *src = (Data::access_t const *)(buffer);

			for (size_t i = 0; i < num_accesses; i++) {
				if (!_wait_for_bwe())
					return false;

				write<Data>(*src++);
			}

			return _wait_for_transfer_complete();
		}

		/**
		 * Read data blocks from SD card via master DMA
		 *
		 * \return true on success
		 */
		bool read_blocks_dma(size_t block_number, size_t block_count,
		                     Genode::addr_t out_buffer_phys)
		{
			using namespace Sd_card;

			write<Blk::Blen>(0x200);
			write<Blk::Nblk>(block_count);

			_setup_adma_descriptor_table(block_count, out_buffer_phys);

			if (!issue_command(Read_multiple_block(block_number))) {
				Genode::error("Read_multiple_block failed, Stat: ",
				              Genode::Hex(read<Stat>()));
				return false;
			}

			return _wait_for_transfer_complete_irq();
		}

		/**
		 * Write data blocks to SD card via master DMA
		 *
		 * \return true on success
		 */
		bool write_blocks_dma(size_t block_number, size_t block_count,
		                      Genode::addr_t buffer_phys)
		{
			using namespace Sd_card;

			write<Blk::Blen>(0x200);
			write<Blk::Nblk>(block_count);

			_setup_adma_descriptor_table(block_count, buffer_phys);

			if (!issue_command(Write_multiple_block(block_number))) {
				Genode::error("Write_multiple_block failed");
				return false;
			}

			return _wait_for_transfer_complete_irq();
		}
};

#endif /* _DRIVERS__SD_CARD__SPEC__OMAP4__MMCHS_H_ */
