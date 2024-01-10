/*
 * \brief  Secured Digital Host Controller
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__SD_CARD__SPEC__IMX__DRIVER_H_
#define _SRC__DRIVERS__SD_CARD__SPEC__IMX__DRIVER_H_

/* Genode includes */
#include <platform_session/device.h>
#include <timer_session/connection.h>

/* local includes */
#include <driver_base.h>
#include <adma2.h>

namespace Sd_card { class Driver; }


class Sd_card::Driver : public  Driver_base,
                        private Platform::Device,
                        private Platform::Device::Mmio<0x100>
{
	using Mmio = Genode::Mmio<SIZE>;

	private:

		enum Bus_width { BUS_WIDTH_1, BUS_WIDTH_4 };

		enum Clock { CLOCK_INITIAL, CLOCK_OPERATIONAL };

		enum Clock_divider { CLOCK_DIV_4, CLOCK_DIV_8, CLOCK_DIV_512 };


		/********************
		 ** MMIO structure **
		 ********************/

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


		/************************
		 ** Utility structures **
		 ************************/

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			Timer_delayer(Genode::Env &env) : Timer::Connection(env) { }

			void usleep(uint64_t us) override { Timer::Connection::usleep(us); }
		};

		struct Block_transfer
		{
			Block::Packet_descriptor packet { };
			bool                     pending = false;
			bool                     read    = false;
		};

		Env                   & _env;
		Platform::Connection  & _platform;
		Block_transfer          _block_transfer { };
		Timer_delayer           _delayer        { _env };
		Signal_handler<Driver>  _irq_handler    { _env.ep(), *this,
		                                          &Driver::_handle_irq };
		Platform::Device::Irq   _irq            { *this };
		Card_info               _card_info      { _init() };
		Adma2::Table            _adma2_table    { _platform };

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
		int  _reset();
		void _reset_amendments();
		int  _wait_for_cmd_allowed();
		int  _wait_for_cmd_complete();
		int  _wait_for_card_ready_mbw();
		int  _stop_transmission();
		void _stop_transmission_finish_xfertyp(Xfertyp::access_t &xfertyp);
		int  _wait_for_cmd_complete_mb_finish(bool const reading);

		int _prepare_dma_mb(Block::Packet_descriptor packet,
		                    bool                     reading,
		                    size_t                   blk_cnt,
		                    addr_t                   buf_phys);

		bool _issue_cmd_finish_xfertyp(Xfertyp::access_t &xfertyp,
		                               bool const         transfer,
		                               bool const         multiblock,
		                               bool const         reading);

		Card_info _init();


		/*********************
		 ** Host_controller **
		 *********************/

		Cid      _read_cid() override;
		Csd      _read_csd() override;
		unsigned _read_rca() override;
		bool     _issue_command(Command_base const & cmd) override;

		Card_info card_info() const override { return _card_info; }

	public:

		using Block::Driver::read;
		using Block::Driver::write;

		Driver(Env &env, Platform::Connection & platform);
		~Driver();


		/*******************
		 ** Block::Driver **
		 *******************/

		void read_dma(Block::sector_t           block_number,
		              size_t                    block_count,
		              addr_t                    phys,
		              Block::Packet_descriptor &packet) override;

		void write_dma(Block::sector_t           block_number,
		               size_t                    block_count,
		               addr_t                    phys,
		               Block::Packet_descriptor &packet) override;

		bool dma_enabled() override { return true; }

		Dma_buffer alloc_dma_buffer(size_t size, Cache cache) override
		{
			Ram_dataspace_capability ds =
				_platform.retry_with_upgrade(Ram_quota{4096}, Cap_quota{2},
					[&] () { return _platform.alloc_dma_buffer(size, cache); });

			return { .ds       = ds,
			         .dma_addr = _platform.dma_addr(ds) };
		}
};

#endif /* _SRC__DRIVERS__SD_CARD__SPEC__IMX__DRIVER_H_ */
