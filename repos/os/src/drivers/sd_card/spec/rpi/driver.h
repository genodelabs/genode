/*
 * \brief  Raspberry Pi SDHCI driver
 * \author Norman Feske
 * \author Timo Wischer
 * \author Martin Stein
 * \date   2014-09-21
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <drivers/defs/rpi.h>
#include <os/attached_mmio.h>
#include <irq_session/connection.h>

/* local includes */
#include <driver_base.h>

namespace Sd_card { class Driver; }


class Sd_card::Driver : public  Driver_base,
                        private Attached_mmio
{
	private:

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
		 * Handle the SDHCI quirk that responses of 136-bit requests are
		 * shifted by 8 bits.
		 */
		template <off_t OFFSET>
		struct Cmdresp_tpl : Register<OFFSET, 32>
		{
			struct Resp_8_24 : Register<OFFSET, 32>::template Bitfield<0, 24> { };
			struct Resp_0_8  : Register<OFFSET, 32>::template Bitfield<24, 8> { };
		};

		struct Cmdresp0  : Cmdresp_tpl<0x10> { };
		struct Cmdresp1  : Cmdresp_tpl<0x14> { };
		struct Cmdresp2  : Cmdresp_tpl<0x18> { };
		struct Cmdresp3  : Cmdresp_tpl<0x1c> { };

		struct Resp0_136 : Bitset_2<Cmdresp3::Resp_0_8, Cmdresp0::Resp_8_24> { };
		struct Resp1_136 : Bitset_2<Cmdresp0::Resp_0_8, Cmdresp1::Resp_8_24> { };
		struct Resp2_136 : Bitset_2<Cmdresp1::Resp_0_8, Cmdresp2::Resp_8_24> { };
		struct Resp3_136 : Bitset_2<Cmdresp2::Resp_0_8, Cmdresp3::Resp_8_24> { };

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

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			void usleep(unsigned us) { Timer::Connection::usleep(us); }
		};

		Ram_session    &_ram;
		Timer_delayer   _delayer;
		Irq_connection  _irq       { Rpi::SDHCI_IRQ };
		Card_info       _card_info { _init() };

		template <typename REG>
		bool _poll_and_wait_for(unsigned value)
		{
			/* poll for a while */
			try { wait_for(Attempts(5000), Microseconds(0), _delayer,
			               typename REG::Equal(value)); }
			catch (Polling_timeout) {

				/* if the value was not reached while polling, start sleeping */
				try { wait_for(_delayer, typename REG::Equal(value)); }
				catch (Polling_timeout) { return false; }
			}
			return true;
		}

		Card_info _init();
		void      _set_and_enable_clock(unsigned divider);
		void      _set_block_count(size_t block_count);
		size_t    _block_to_command_address(size_t block_number);


		/*********************
		 ** Host_controller **
		 *********************/

		bool _issue_command(Command_base const &command) override;
		Cid  _read_cid() override;
		Csd  _read_csd() override;

		Card_info card_info() const override { return _card_info; }

		unsigned _read_rca() override {
			return Send_relative_addr::Response::Rca::get(Mmio::read<Resp0>()); }

	public:

		Driver(Env &env);


		/*******************
		 ** Block::Driver **
		 *******************/

		void read(Block::sector_t           block_number,
		          size_t                    block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet) override;

		void write(Block::sector_t           block_number,
		           size_t                    block_count,
		           char const               *buffer,
		           Block::Packet_descriptor &packet) override;

		Ram_dataspace_capability alloc_dma_buffer(size_t size) override {
			return _ram.alloc(size, UNCACHED); }
};

#endif /* _DRIVER_H_ */
