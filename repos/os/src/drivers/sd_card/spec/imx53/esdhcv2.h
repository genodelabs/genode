/*
 * \brief  Freescale Enhanced Secured Digital Host Controller Version 2
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ESDHCV2_H_
#define _ESDHCV2_H_

/* Genode includes */
#include <util/mmio.h>
#include <irq_session/connection.h>
#include <base/sleep.h>

/* local includes */
#include <sd_card.h>
#include <adma2.h>

namespace Genode
{
	struct Esdhcv2;
	class  Esdhcv2_controller;
}

/**
 * MMIO structure of a Freescale ESDHCv2
 */
struct Genode::Esdhcv2 : Mmio
{
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
	struct Xfertyp  : Register<0xc, 32>
	{
		struct Dmaen  : Bitfield<0, 1> { };
		struct Bcen   : Bitfield<1, 1> { };
		struct Ac12en : Bitfield<2, 1> { };
		struct Dtdsel : Bitfield<4, 1>
		{
			enum { WRITE = 0, READ  = 1, };
		};
		struct Msbsel : Bitfield<5, 1> { };
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
	struct Prsstat     : Register<0x24, 32> { };
	struct Prsstat_lhw : Register<0x24, 16>
	{
		struct Sdstb : Bitfield<3, 1> { };

		static constexpr access_t cmd_allowed() { return Sdstb::reg_mask(); }
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
			enum { SDCLK_TIMES_2_POW_27 = 0xe };
		};
		struct Rsta : Bitfield<24, 1> { };
		struct Rstc : Bitfield<25, 1> { };
		struct Rstd : Bitfield<26, 1> { };
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

	Esdhcv2(addr_t const mmio_base) : Mmio(mmio_base) { }
};

/**
 * Implementation of the SD host-controller interface for the ESDHCv2
 */
struct Genode::Esdhcv2_controller
:
	private Esdhcv2, public Sd_card::Host_controller
{
	private:

		enum {
			BLOCK_SIZE      = 512,
			WATERMARK_WORDS = 16,
			BURST_WORDS     = 8,
		};

		enum Bus_width { BUS_WIDTH_1, BUS_WIDTH_4 };

		enum Clock_divider { CLOCK_DIV_8, CLOCK_DIV_512 };

		Irq_connection     _irq;
		Signal_receiver    _irq_rec;
		Signal_context     _irq_ctx;
		Delayer &          _delayer;
		Sd_card::Card_info _card_info;
		bool const         _use_dma;
		Adma2::Table       _adma2_table;

		void _detect_err(char const * const err);
		void _disable_irqs();
		void _enable_irqs();
		void _bus_width(Bus_width bus_width);
		void _disable_clock();
		void _enable_clock(Clock_divider divider, Delayer &delayer);
		void _clock(enum Clock_divider divider, Delayer &delayer);
		void _wait_for_irq();
		int  _reset(Delayer & delayer);
		int  _wait_for_cmd_allowed();
		int  _wait_for_cmd_complete();
		int  _wait_for_card_ready_mbw();
		int  _stop_transmission_mbw();
		int  _wait_for_cmd_complete_mb(bool const r);
		int  _prepare_dma_mb(size_t blk_cnt, addr_t buf_phys);

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
		 *
		 * \param base       local base address of MMIO registers
		 * \param irq        host-interrupt ID
		 * \param delayer    delayer timing of MMIO accesses
		 * \param use_dma    wether to use DMA or direct IO for transfers
		 */
		Esdhcv2_controller(addr_t const base, unsigned const irq,
		                   Delayer & delayer, bool const use_dma);

		~Esdhcv2_controller() { _irq_rec.dissolve(&_irq_ctx); }


		/****************************************
		 ** Sd_card::Host_controller interface **
		 ****************************************/

		bool read_blocks(size_t, size_t, char *);
		bool write_blocks(size_t, size_t, char const *);
		bool read_blocks_dma(size_t blk_nr, size_t blk_cnt, addr_t buf_phys);
		bool write_blocks_dma(size_t blk_nr, size_t blk_cnt, addr_t buf_phys);

		Sd_card::Card_info card_info() const { return _card_info; }
};

#endif /* _ESDHCV2_H_ */
