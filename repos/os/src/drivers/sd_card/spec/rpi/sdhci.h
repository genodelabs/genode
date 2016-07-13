/*
 * \brief  SDHCI controller driver
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Timo Wischer
 * \date   2014-09-21
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__SD_CARD__SPEC__RPI__SDHCI_H_
#define _DRIVERS__SD_CARD__SPEC__RPI__SDHCI_H_

/* Genode includes */
#include <util/mmio.h>
#include <os/attached_ram_dataspace.h>
#include <irq_session/connection.h>
#include <drivers/board_base.h>

/* local includes */
#include <sd_card.h>


struct Sdhci : Genode::Mmio
{
	enum { verbose = false };

	typedef Genode::size_t size_t;

	struct Blksizecnt : Register<0x4, 32>
	{
		struct Blkcnt  : Bitfield<16, 16> { };
		struct Blksize : Bitfield<0,  10> { };
	};

	struct Resp0 : Register<0x10, 32> { };
	struct Resp1 : Register<0x14, 32> { };
	struct Resp2 : Register<0x18, 32> { };
	struct Resp3 : Register<0x1c, 32> { };

	/*
	 * Handle the SDHCI quirk that responses of 136-bit requests are shifted
	 * by 8 bits.
	 */
	template <Genode::off_t OFFSET>
	struct Cmdresp_tpl : Register<OFFSET, 32>
	{
		struct Resp_8_24 : Register<OFFSET, 32>::template Bitfield<0, 24> { };
		struct Resp_0_8  : Register<OFFSET, 32>::template Bitfield<24, 8> { };
	};
	struct Cmdresp0  : Cmdresp_tpl<0x10> { };
	struct Cmdresp1  : Cmdresp_tpl<0x14> { };
	struct Cmdresp2  : Cmdresp_tpl<0x18> { };
	struct Cmdresp3  : Cmdresp_tpl<0x1c> { };

	struct Resp0_136 : Genode::Bitset_2<Cmdresp3::Resp_0_8, Cmdresp0::Resp_8_24> { };
	struct Resp1_136 : Genode::Bitset_2<Cmdresp0::Resp_0_8, Cmdresp1::Resp_8_24> { };
	struct Resp2_136 : Genode::Bitset_2<Cmdresp1::Resp_0_8, Cmdresp2::Resp_8_24> { };
	struct Resp3_136 : Genode::Bitset_2<Cmdresp2::Resp_0_8, Cmdresp3::Resp_8_24> { };

	struct Data : Register<0x20, 32> { };

	struct Control0 : Register<0x28, 32>
	{
		struct Hctl_dwidth : Bitfield<1, 1> { };
		struct Hctl_hs_en  : Bitfield<2, 1> { };
	};

	struct Control1 : Register<0x2c, 32>
	{
		struct Clk_internal_en     : Bitfield<0, 1> { };
		struct Clk_internal_stable : Bitfield<1, 1> { };
		struct Clk_en              : Bitfield<2, 1> { };

		struct Clk_freq8    : Bitfield<8,  8> { };
		struct Clk_freq_ms2 : Bitfield<6,  2> { };
		struct Data_tounit  : Bitfield<16, 4> { };
		struct Srst_hc      : Bitfield<24, 1> { };
		struct Srst_cmd     : Bitfield<25, 1> { };
		struct Srst_data    : Bitfield<26, 1> { };
	};

	struct Status : Register<0x24, 32>
	{
		struct Inhibit : Bitfield<0,  2> { };
		struct Bwe     : Bitfield<10, 1> { };
		struct Bre     : Bitfield<11, 1> { };
	};

	struct Host_ctrl : Register<0x28, 32>
	{
		struct Voltage : Bitfield<9,  3> {
			enum {
				V18 = 0b101,
				V30 = 0b110,
				V33 = 0b111,
			};
		};
		struct Power   : Bitfield<8,  1> { };
	};

	struct Arg1 : Register<0x8, 32> { };

	struct Cmdtm : Register<0xc, 32>
	{
		struct Index : Bitfield<24, 6> { };
		struct Isdata : Bitfield<21, 1> { };
		struct Tm_blkcnt_en : Bitfield<1, 1> { };
		struct Tm_multi_block : Bitfield<5, 1> { };
		struct Tm_auto_cmd_en : Bitfield<2, 2>
		{
			enum { CMD12 = 1 };
		};
		struct Tm_dat_dir : Bitfield<4, 1>
		{
			enum { WRITE = 0, READ = 1 };
		};
		struct Rsp_type : Bitfield<16, 2>
		{
			enum Response { RESPONSE_NONE             = 0,
			                RESPONSE_136_BIT          = 1,
			                RESPONSE_48_BIT           = 2,
			                RESPONSE_48_BIT_WITH_BUSY = 3 };
		};
	};

	struct Interrupt : Register<0x30, 32>
	{
		struct Cmd_done  : Bitfield<0, 1> { };
		struct Data_done : Bitfield<1, 1> { };
	};

	struct Irpt_mask : Register<0x34, 32> { };
	struct Irpt_en   : Register<0x38, 32> { };

	struct Capabilities : Register<0x40, 32> { };

	struct Host_version : Register<0xFE, 16>
	{
		struct Spec     : Bitfield<0, 8> { };
		struct Vendor   : Bitfield<8, 8> { };
	};

	Sdhci(Genode::addr_t const mmio_base) : Genode::Mmio(mmio_base) { }
};


struct Sdhci_controller : private Sdhci, public Sd_card::Host_controller
{
	enum { Block_size = 0x200 };

	private:

		Delayer           &_delayer;
		Sd_card::Card_info _card_info;

		Genode::Irq_connection _irq;

		void _set_and_enable_clock(unsigned divider)
		{
			Control1::access_t v = read<Control1>();
			Control1::Clk_freq8::set(v, divider);
			Control1::Clk_freq_ms2::set(v, 0);
			Control1::Clk_internal_en::set(v, 1);

			write<Control1>(v);

			if (!wait_for<Control1::Clk_internal_stable>(1, _delayer)) {
				Genode::error("could not set internal clock");
				throw Detection_failed();
			}

			write<Control1::Clk_en>(1);

			_delayer.usleep(10*1000);

			/* data timeout unit exponent */
			write<Control1::Data_tounit>(0xe);
		}

		Sd_card::Card_info _init(const bool set_voltage)
		{
			using namespace Sd_card;

			/* reset host controller */
			{
				Control1::access_t v = read<Control1>();
				Control1::Srst_hc::set(v);
				Control1::Srst_data::set(v);
				write<Control1>(v);
			}

			if (!wait_for<Control1::Srst_hc>(0, _delayer)) {
				Genode::error("host-controller soft reset timed out");
				throw Detection_failed();
			}

			Genode::log("SDHCI version: ", read<Host_version::Vendor>(), " "
			            "(specification ", read<Host_version::Spec>() + 1, ".0)");

			/*
			 * Raspberry Pi (BCM2835) does not need to
			 * set the sd card voltage and
			 * power up the host controller.
			 * This registers are reserved and
			 * always have to be written to 0.
			 */
			if (set_voltage) {
				/* Enable sd card power */
				write<Host_ctrl>(Host_ctrl::Power::bits(1)
				               | Host_ctrl::Voltage::bits(Host_ctrl::Voltage::V33));
			}

			/* enable interrupt status reporting */
			write<Irpt_mask>(~0UL);
			write<Irpt_en>(~0UL);

			/*
			 * We don't read the capability register as the BCM2835 always
			 * returns all bits set to zero.
			 */

			_set_and_enable_clock(240);

			if (!issue_command(Go_idle_state())) {
				Genode::warning("Go_idle_state command failed");
				throw Detection_failed();
			}

			_delayer.usleep(2000);

			if (!issue_command(Send_if_cond())) {
				Genode::warning("Send_if_cond command failed");
				throw Detection_failed();
			}

			if (read<Resp0>() != 0x1aa) {
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

				if (Sd_card::Ocr::Busy::get(read<Resp0>()))
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

			/* switch host controller to use 4 data signals */
			{
				Control0::access_t v = read<Control0>();
				Control0::Hctl_dwidth::set(v);
				Control0::Hctl_hs_en::set(v);
				write<Control0>(v);
			}

			_delayer.usleep(10*1000);

			/*
			 * Accelerate clock, the divider is hard-coded for now.
			 *
			 * The Raspberry Pi report as clock of 250 MHz. According to the
			 * SDHCI specification, it is possible to driver SD cards with
			 * 50 MHz in high-speed mode (Hctl_hs_en).
			 */
			_set_and_enable_clock(5);

			return card_info;
		}

		/**
		 * Define the block count for the next data transfer
		 */
		void _set_block_count(size_t block_count)
		{
			/*
			 * The 'Blksizecnt' register must be written in one step. If we
			 * used subsequent writes for the 'Blkcnt' and 'Blksize' bitfields,
			 * the host controller of the BCM2835 would fail to recognize any
			 * but the first write operation.
			 */
			Blksizecnt::access_t v = read<Blksizecnt>();
			Blksizecnt::Blkcnt::set(v, block_count);
			Blksizecnt::Blksize::set(v, Block_size);
			write<Blksizecnt>(v);
		}

		template <typename REG>
		bool _poll_and_wait_for(unsigned value)
		{
			/* poll for a while */
			if (wait_for<REG>(value, _delayer, 5000, 0))
				return true;

			/* if the value were not reached while polling, start sleeping */
			return wait_for<REG>(value, _delayer);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param mmio_base  local base address of MMIO registers
		 */
		Sdhci_controller(Genode::addr_t const mmio_base, Delayer &delayer,
						 unsigned irq, bool use_dma, const bool set_voltage = false)
		:
			Sdhci(mmio_base), _delayer(delayer), _card_info(_init(set_voltage)), _irq(irq)
		{ }


		/****************************************
		 ** Sd_card::Host_controller interface **
		 ****************************************/

		bool _issue_command(Sd_card::Command_base const &command)
		{
			if (verbose)
				Genode::log("-> ", command);

			if (!_poll_and_wait_for<Status::Inhibit>(0)) {
				Genode::error("controller inhibits issueing commands");
				return false;
			}

			/* write command argument */
			write<Arg1>(command.arg);

			/* assemble command register */
			Cmdtm::access_t cmd = 0;
			Cmdtm::Index::set(cmd, command.index);
			if (command.transfer != Sd_card::TRANSFER_NONE) {

				Cmdtm::Isdata::set(cmd);
				Cmdtm::Tm_blkcnt_en::set(cmd);
				Cmdtm::Tm_multi_block::set(cmd);

				if (command.index == Sd_card::Read_multiple_block::INDEX
				 || command.index == Sd_card::Write_multiple_block::INDEX) {
					Cmdtm::Tm_auto_cmd_en::set(cmd, Cmdtm::Tm_auto_cmd_en::CMD12);
				}

				/* set data-direction bit depending on the command */
				bool const read = command.transfer == Sd_card::TRANSFER_READ;
				Cmdtm::Tm_dat_dir::set(cmd, read ? Cmdtm::Tm_dat_dir::READ
				                                 : Cmdtm::Tm_dat_dir::WRITE);
			}

			Cmdtm::access_t rsp_type = 0;
			switch (command.rsp_type) {
			case Sd_card::RESPONSE_NONE:             rsp_type = Cmdtm::Rsp_type::RESPONSE_NONE;             break;
			case Sd_card::RESPONSE_136_BIT:          rsp_type = Cmdtm::Rsp_type::RESPONSE_136_BIT;          break;
			case Sd_card::RESPONSE_48_BIT:           rsp_type = Cmdtm::Rsp_type::RESPONSE_48_BIT;           break;
			case Sd_card::RESPONSE_48_BIT_WITH_BUSY: rsp_type = Cmdtm::Rsp_type::RESPONSE_48_BIT_WITH_BUSY; break;
			}
			Cmdtm::Rsp_type::set(cmd, rsp_type);

			/* write command */
			write<Cmdtm>(cmd);

			if (!_poll_and_wait_for<Interrupt::Cmd_done>(1)) {
				Genode::error("command timed out");
				return false;
			}

			/* clear interrupt state */
			write<Interrupt::Cmd_done>(1);

			return true;
		}

		Sd_card::Card_info card_info() const
		{
			return _card_info;
		}

		Sd_card::Cid _read_cid()
		{
			Sd_card::Cid cid;
			cid.raw_0 = read<Resp0_136>();
			cid.raw_1 = read<Resp1_136>();
			cid.raw_2 = read<Resp2_136>();
			cid.raw_3 = read<Resp3_136>();
			return cid;
		}

		Sd_card::Csd _read_csd()
		{
			Sd_card::Csd csd;
			csd.csd0 = read<Resp0_136>();
			csd.csd1 = read<Resp1_136>();
			csd.csd2 = read<Resp2_136>();
			csd.csd3 = read<Resp3_136>();
			return csd;
		}

		unsigned _read_rca()
		{
			return Sd_card::Send_relative_addr::Response::Rca::get(read<Resp0>());
		}

		size_t _block_to_command_address(const size_t block_number)
		{
			/* use byte position for addressing with standard cards */
			if (_card_info.version() == Sd_card::Csd3::Version::STANDARD_CAPACITY) {
				return block_number * Block_size;
			}

			return block_number;
		}

		/**
		 * Read data blocks from SD card
		 *
		 * \return true on success
		 */
		bool read_blocks(size_t block_number, size_t block_count, char *out_buffer)
		{
			using namespace Sd_card;

			_set_block_count(block_count);

			if (!issue_command(Read_multiple_block(_block_to_command_address(block_number)))) {
				Genode::error("Read_multiple_block failed, Status: ",
				              Genode::Hex(read<Status>()));
				return false;
			}

			Data::access_t *dst = (Data::access_t *)(out_buffer);
			for (size_t i = 0; i < block_count; i++) {

				/*
				 * Check for buffer-read enable bit for each block
				 *
				 * According to the BCM2835 documentation, this bit is
				 * reserved but it actually corresponds to the bre status
				 * bit as described in the SDHCI specification.
				 */
				if (!_poll_and_wait_for<Status::Bre>(1))
					return false;

				/* read data from sdhci buffer */
				for (size_t j = 0; j < 512/sizeof(Data::access_t); j++)
					*dst++ = read<Data>();
			}

			if (!_poll_and_wait_for<Interrupt::Data_done>(1)) {
				Genode::error("completion of read request failed (interrupt "
				              "status ", Genode::Hex(read<Interrupt>()), ")");
				return false;
			}

			/* clear interrupt state */
			write<Interrupt::Data_done>(1);

			return true;
		}

		/**
		 * Write data blocks to SD card
		 *
		 * \return true on success
		 */
		bool write_blocks(size_t block_number, size_t block_count, char const *buffer)
		{
			using namespace Sd_card;

			_set_block_count(block_count);

			if (!issue_command(Write_multiple_block(_block_to_command_address(block_number)))) {
				Genode::error("Write_multiple_block failed, Status: ",
				              Genode::Hex(read<Status>()));
				return false;
			}

			Data::access_t const *src = (Data::access_t const *)(buffer);
			for (size_t i = 0; i < block_count; i++) {

				/* check for buffer-write enable bit for each block */
				if (!_poll_and_wait_for<Status::Bwe>(1))
					return false;

				/* write data into sdhci buffer */
				for (size_t j = 0; j < 512/sizeof(Data::access_t); j++)
					write<Data>(*src++);
			}

			if (!_poll_and_wait_for<Interrupt::Data_done>(1)) {
				Genode::error("completion of write request failed (interrupt "
				              "status ", read<Interrupt>(), ")");
				return false;
			}

			/* clear interrupt state */
			write<Interrupt::Data_done>(1);

			return true;
		}

		/**
		 * Read data blocks from SD card via master DMA
		 *
		 * \return true on success
		 */
		bool read_blocks_dma(size_t block_number, size_t block_count,
		                     Genode::addr_t out_buffer_phys)
		{
			return false;
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

			return false;
		}
};

#endif /* _DRIVERS__SD_CARD__SPEC__RPI__SDHCI_H_ */
