/*
 * \brief  Secured Digital Host Controller
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SDHC_H_
#define _SDHC_H_

/* Genode includes */
#include <base/log.h>
#include <os/attached_io_mem_dataspace.h>
#include <timer_session/connection.h>
#include <util/mmio.h>
#include <irq_session/connection.h>
#include <base/sleep.h>
#include <block/driver.h>
#include <block/component.h>
#include <drivers/board_base.h>
#include <os/attached_mmio.h>

/* local includes */
#include <sd_card.h>
#include <adma2.h>

namespace Genode { class Sdhc; }


class Genode::Sdhc : public Attached_mmio,
                     public Block::Driver,
                     public Sd_card::Host_controller
{
	private:

		struct Blkattr : Register<0x4, 32>
		{
			struct Blksize : Bitfield<0, 13> { };
			struct Blkcnt  : Bitfield<16, 16> { };
		};
		template <off_t OFFSET>
		struct Cmdrsp_tpl : Register<OFFSET, 32>
		{
			struct Rsp136_8_24 : Register<OFFSET, 32>::template Bitfield<0, 24> { };
			struct Rsp136_0_8  : Register<OFFSET, 32>::template Bitfield<24, 8> { };
		};
		struct Cmdarg   : Register<0x8, 32> { };
		struct Cmdrsp0  : Cmdrsp_tpl<0x10> { };
		struct Cmdrsp1  : Cmdrsp_tpl<0x14> { };
		struct Cmdrsp2  : Cmdrsp_tpl<0x18> { };
		struct Cmdrsp3  : Cmdrsp_tpl<0x1c> { };
		struct Rsp136_0 : Bitset_2<Cmdrsp3::Rsp136_0_8, Cmdrsp0::Rsp136_8_24> { };
		struct Rsp136_1 : Bitset_2<Cmdrsp0::Rsp136_0_8, Cmdrsp1::Rsp136_8_24> { };
		struct Rsp136_2 : Bitset_2<Cmdrsp1::Rsp136_0_8, Cmdrsp2::Rsp136_8_24> { };
		struct Rsp136_3 : Bitset_2<Cmdrsp2::Rsp136_0_8, Cmdrsp3::Rsp136_8_24> { };

		template <off_t OFFSET>
		struct Xfertyp_base : Register<OFFSET, 32>
		{
			struct Dmaen  : Register<OFFSET, 32>::template Bitfield<0, 1> { };
			struct Bcen   : Register<OFFSET, 32>::template Bitfield<1, 1> { };
			struct Ac12en : Register<OFFSET, 32>::template Bitfield<2, 1> { };
			struct Dtdsel : Register<OFFSET, 32>::template Bitfield<4, 1>
			{
				enum { WRITE = 0, READ  = 1, };
			};
			struct Msbsel : Register<OFFSET, 32>::template Bitfield<5, 1> { };
		};
		struct Mixctrl : Xfertyp_base<0x48>
		{
			struct Ddren       : Bitfield<3, 1> { };
			struct Nibblepos   : Bitfield<6, 1> { };
			struct Ac23en      : Bitfield<7, 1> { };
			struct Always_ones : Bitfield<31, 1> { };
		};
		struct Xfertyp : Xfertyp_base<0xc>
		{
			struct Rsptyp : Bitfield<16, 2>
			{
				enum {
					_0BIT       = 0,
					_136BIT     = 1,
					_48BIT      = 2,
					_48BIT_BUSY = 3,
				};
			};
			struct Cccen  : Bitfield<19, 1> { };
			struct Cicen  : Bitfield<20, 1> { };
			struct Dpsel  : Bitfield<21, 1> { };
			struct Cmdtyp : Bitfield<22, 2>
			{
				enum { ABORT_CMD12 = 3 };
			};
			struct Cmdinx : Bitfield<24, 6> { };
		};
		struct Prsstat     : Register<0x24, 32>
		{
			struct Cihb  : Bitfield<0, 1> { };
			struct Cdihb : Bitfield<1, 1> { };
			struct Dla   : Bitfield<2, 1> { };
			struct Sdstb : Bitfield<3, 1> { };
		};
		struct Proctl : Register<0x28, 32>
		{
			struct Dtw  : Bitfield<1, 2>
			{
				enum { _1BIT = 0, _4BIT = 1 };
			};
			struct Dmas : Bitfield<8, 2> { enum { ADMA2 = 2 }; };
		};
		struct Sysctl : Register<0x2c, 32>
		{
			struct Ipgen : Bitfield<0, 1> { };
			struct Hcken : Bitfield<1, 1> { };
			struct Peren : Bitfield<2, 1> { };
			struct Dvs   : Bitfield<4, 4>
			{
				enum { DIV1  = 0x0, DIV4  = 0x3, DIV16 = 0xf, };
			};
			struct Sdclkfs : Bitfield<8, 8>
			{
				enum { DIV1  = 0x00, DIV2  = 0x01, DIV32 = 0x10, };
			};
			struct Dtocv : Bitfield<16, 4>
			{
				enum {
					SDCLK_TIMES_2_POW_28 = 0xf,
					SDCLK_TIMES_2_POW_27 = 0xe,
					SDCLK_TIMES_2_POW_13 = 0x0,
				};
			};
			struct Ipp_rst_n : Bitfield<23, 1> { };
			struct Rsta      : Bitfield<24, 1> { };
			struct Rstc      : Bitfield<25, 1> { };
			struct Rstd      : Bitfield<26, 1> { };
		};

		template <off_t OFFSET>
		struct Irq_tpl : Register<OFFSET, 32>
		{
			struct Cc     : Register<OFFSET, 32>::template Bitfield<0, 1> { };
			struct Tc     : Register<OFFSET, 32>::template Bitfield<1, 1> { };
			struct Dint   : Register<OFFSET, 32>::template Bitfield<3, 1> { };
			struct Ctoe   : Register<OFFSET, 32>::template Bitfield<16, 1> { };
			struct Cce    : Register<OFFSET, 32>::template Bitfield<17, 1> { };
			struct Cebe   : Register<OFFSET, 32>::template Bitfield<18, 1> { };
			struct Cie    : Register<OFFSET, 32>::template Bitfield<19, 1> { };
			struct Dtoe   : Register<OFFSET, 32>::template Bitfield<20, 1> { };
			struct Dce    : Register<OFFSET, 32>::template Bitfield<21, 1> { };
			struct Debe   : Register<OFFSET, 32>::template Bitfield<22, 1> { };
			struct Ac12e  : Register<OFFSET, 32>::template Bitfield<24, 1> { };
			struct Dmae   : Register<OFFSET, 32>::template Bitfield<28, 1> { };
		};
		struct Irq        : Irq_tpl<0> { };
		struct Irqstat    : Irq_tpl<0x30> { };
		struct Irqstaten  : Irq_tpl<0x34> { };
		struct Irqsigen   : Irq_tpl<0x38> { };
		struct Maxcurrent : Register<0x48, 32> { };
		struct Adsaddr    : Register<0x58, 32> { };
		struct Hostver : Register<0xfc, 32>
		{
			struct Svn : Bitfield<0, 8> { };
			struct Vvn : Bitfield<8, 8> { };
		};
		struct Wml : Register<0x44, 32>
		{
			struct Rd_wml      : Bitfield<0, 8> { };
			struct Rd_brst_len : Bitfield<8, 5> { };
			struct Wr_wml      : Bitfield<16, 8> { };
			struct Wr_brst_len : Bitfield<24, 5> { };
		};
		struct Vendspec : Register<0xc0, 32>
		{
			struct Frc_sdclk_on : Bitfield<8, 1> { };
		};

		enum { BLOCK_SIZE = 512 };

		enum Bus_width { BUS_WIDTH_1, BUS_WIDTH_4 };

		enum Clock { CLOCK_INITIAL, CLOCK_OPERATIONAL };

		enum Clock_divider { CLOCK_DIV_4, CLOCK_DIV_8, CLOCK_DIV_512 };

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			void usleep(unsigned us) { Timer::Connection::usleep(us); }

		} _delayer;

		struct Block_transfer
		{
			Block::Packet_descriptor packet;
			bool                     pending = false;
			bool                     read;

		} _block_transfer;

		Signal_handler<Sdhc> _irq_handler;
		Irq_connection       _irq;
		Sd_card::Card_info   _card_info;
		bool const           _use_dma;
		Adma2::Table         _adma2_table;

		static bool _supported_host_version(Hostver::access_t hostver);
		static void _watermark_level(Wml::access_t &wml);

		void _handle_irq();
		void _detect_err(char const * const err);
		void _disable_irqs();
		void _enable_irqs();
		void _bus_width(Bus_width bus_width);
		void _disable_clock();
		void _disable_clock_preparation();
		void _enable_clock(Clock_divider divider);
		void _enable_clock_finish();
		void _clock(Clock clock);
		void _clock_finish(Clock clock);
		int  _reset(Delayer & delayer);
		void _reset_amendments();
		int  _wait_for_cmd_allowed();
		int  _wait_for_cmd_complete();
		int  _wait_for_card_ready_mbw();
		int  _stop_transmission();
		void _stop_transmission_finish_xfertyp(Xfertyp::access_t &xfertyp);
		int  _wait_for_cmd_complete_mb_finish(bool const reading);
		int  _prepare_dma_mb(Block::Packet_descriptor packet, bool reading, size_t blk_cnt, addr_t buf_phys);
		bool _issue_cmd_finish_xfertyp(Xfertyp::access_t &xfertyp,
		                               bool const         transfer,
		                               bool const         multiblock,
		                               bool const         reading);

		Sd_card::Card_info _init();


		/****************************************
		 ** Sd_card::Host_controller interface **
		 ****************************************/

		Sd_card::Cid _read_cid();
		Sd_card::Csd _read_csd();
		unsigned     _read_rca();
		bool         _issue_command(Sd_card::Command_base const & command);

	public:

		/**
		 * Constructor
		 */
		Sdhc(Env &env);


		/****************************************
		 ** Sd_card::Host_controller interface **
		 ****************************************/

		bool read_blocks(size_t, size_t, char *);
		bool write_blocks(size_t, size_t, char const *);
		bool read_blocks_dma(Block::Packet_descriptor packet, size_t blk_nr, size_t blk_cnt, addr_t buf_phys);
		bool write_blocks_dma(Block::Packet_descriptor packet, size_t blk_nr, size_t blk_cnt, addr_t buf_phys);

		Sd_card::Card_info card_info() const { return _card_info; }



		/*****************************
		 ** Block::Driver interface **
		 *****************************/

		Genode::size_t block_size() { return 512; }

		virtual Block::sector_t block_count()
		{
			return card_info().capacity_mb() * 1024 * 2;
		}

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			o.set_operation(Block::Packet_descriptor::WRITE);
			return o;
		}

		void read(Block::sector_t    block_number,
		          Genode::size_t     block_count,
		          char              *out_buffer,
		          Block::Packet_descriptor &packet)
		{
//			if (!read_blocks(block_number, block_count, out_buffer))
				throw Io_error();
		}

		void write(Block::sector_t    block_number,
		           Genode::size_t     block_count,
		           char const        *buffer,
		           Block::Packet_descriptor &packet)
		{
//			if (!write_blocks(block_number, block_count, buffer))
				throw Io_error();
		}

		void read_dma(Block::sector_t    block_number,
		              Genode::size_t     block_count,
		              Genode::addr_t     phys,
		              Block::Packet_descriptor &packet)
		{
			if (!read_blocks_dma(packet, block_number, block_count, phys))
				throw Io_error();
		}

		void write_dma(Block::sector_t    block_number,
		               Genode::size_t     block_count,
		               Genode::addr_t     phys,
		               Block::Packet_descriptor &packet)
		{
			if (!write_blocks_dma(packet, block_number, block_count, phys))
				throw Io_error();
		}

		bool dma_enabled() { return _use_dma; }

		Genode::Ram_dataspace_capability alloc_dma_buffer(Genode::size_t size) {
			return Genode::env()->ram_session()->alloc(size, UNCACHED); }

		void free_dma_buffer(Genode::Ram_dataspace_capability c) {
			return Genode::env()->ram_session()->free(c); }
};

namespace Block { using Sdhci_driver = Genode::Sdhc; }

#endif /* _SDHC_H_ */
