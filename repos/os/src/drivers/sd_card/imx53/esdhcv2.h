/*
 * \brief  ESDHCv2 host controller
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ESDHCV2_H_
#define _ESDHCV2_H_

/* Genode includes */
#include <util/mmio.h>
#include <os/attached_ram_dataspace.h>
#include <irq_session/connection.h>
#include <base/sleep.h>

/* local includes */
#include <sd_card.h>

namespace Adma2
{
	using namespace Genode;

	/**
	 * Descriptor layout
	 */
	struct Desc : Register<64>
	{
		static size_t constexpr max_size = 64 * 1024 - 4;

		struct Valid   : Bitfield<0, 1> { };
		struct End     : Bitfield<1, 1> { };
		struct Int     : Bitfield<2, 1> { };
		struct Act1    : Bitfield<4, 1> { };
		struct Act2    : Bitfield<5, 1> { };
		struct Length  : Bitfield<16, 16> { };
		struct Address : Bitfield<32, 32> { };
	};

	/**
	 * Descriptor table
	 */
	class Table
	{
		static size_t constexpr _max_desc = 1024;
		static size_t constexpr _ds_size  = _max_desc * sizeof(Desc::access_t);

		Attached_ram_dataspace _ds;
		Desc::access_t * const _base_virt;
		addr_t const           _base_phys;

		public:

			/**
			 * Constructor
			 */
			Table() :
				_ds(env()->ram_session(), _ds_size, UNCACHED),
				_base_virt(_ds.local_addr<Desc::access_t>()),
				_base_phys(Dataspace_client(_ds.cap()).phys_addr())
			{ }

			/**
			 * Marshal descriptors according to block request
			 *
			 * \param size         request size in bytes
			 * \param buffer_phys  physical base of transfer buffer
			 *
			 * \return true on success
			 */
			bool setup_req(size_t const size, addr_t const buffer_phys)
			{
				/*
				 * Sanity check
				 *
				 * FIXME An alternative to this sanity check would be to
				 * expose the maximum transfer size to the driver and let
				 * the driver partition large requests into ones that are
				 * supported.
				 */
				static size_t constexpr max_size = _max_desc * Desc::max_size;
				if (size > max_size) {
					PERR("Block request too large");
					return false;
				}
				/* install new descriptors till they cover all requested bytes */
				addr_t consumed = 0;
				for (int index = 0; consumed < size; index++) {

					/* clamp current request to maximum request size */
					size_t const remaining = size - consumed;
					size_t const curr = min(Desc::max_size, remaining);

					/* assemble new descriptor */
					Desc::access_t desc = 0;
					Desc::Address::set(desc, buffer_phys + consumed);
					Desc::Length::set(desc, curr);
					Desc::Act1::set(desc, 0);
					Desc::Act2::set(desc, 1);
					Desc::Valid::set(desc, 1);

					/* let last descriptor generate transfer-complete signal */
					if (consumed + curr == size) { Desc::End::set(desc, 1); }

					/* install and account descriptor */
					_base_virt[index] = desc;
					consumed += curr;
				}
				return true;
			}

			/*
			 * Accessors
			 */

			addr_t base_phys() const { return _base_phys; }
	};

}

/**
 * Freescale eSDHCv2 host MMIO
 */
struct Esdhcv2 : Genode::Mmio
{
	typedef Genode::size_t size_t;
	typedef Genode::addr_t addr_t;

	/*
	 * MMIO structure
	 */

	struct Blkattr : Register<0x4, 32>
	{
		struct Blksize : Bitfield<0, 13> { };
		struct Blkcnt  : Bitfield<16, 16> { };
	};
	template <Genode::off_t OFFSET>
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
	struct Rsp136_0 : Genode::Bitset_2<Cmdrsp3::Rsp136_0_8, Cmdrsp0::Rsp136_8_24> { };
	struct Rsp136_1 : Genode::Bitset_2<Cmdrsp0::Rsp136_0_8, Cmdrsp1::Rsp136_8_24> { };
	struct Rsp136_2 : Genode::Bitset_2<Cmdrsp1::Rsp136_0_8, Cmdrsp2::Rsp136_8_24> { };
	struct Rsp136_3 : Genode::Bitset_2<Cmdrsp2::Rsp136_0_8, Cmdrsp3::Rsp136_8_24> { };
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
		struct Dpsel  : Bitfield<21, 1> { };
		struct Cmdtyp : Bitfield<22, 2>
		{
			enum { ABORT_CMD12 = 3 };
		};
		struct Cmdinx : Bitfield<24, 6> { };
	};
	struct Datport     : Register<0x20, 32> { };
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

	template <Genode::off_t OFFSET>
	struct Irq_tpl : Register<OFFSET, 32>
	{
		struct Cc     : Register<OFFSET, 32>::template Bitfield<0, 1> { };
		struct Tc     : Register<OFFSET, 32>::template Bitfield<1, 1> { };
		struct Dint   : Register<OFFSET, 32>::template Bitfield<03, 1> { };
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

	/**
	 * Constructor
	 */
	Esdhcv2(addr_t const mmio_base) : Genode::Mmio(mmio_base) { }

	/**
	 * Reset command line circuit of host controller
	 */
	bool reset_command(Delayer & delayer)
	{
		write<Sysctl::Rstc>(1);
		if (!wait_for<Sysctl::Rstc>(0, delayer)) {
			PERR("Reset of command circuit failed");
			return false;
		}
		return true;
	}

	/**
	 * Reset data circuit of host controller
	 */
	bool reset_data(Delayer & delayer)
	{
		write<Sysctl::Rstd>(1);
		if (!wait_for<Sysctl::Rstd>(0, delayer)) {
			PERR("Reset of data circuit failed");
			return false;
		}
		return true;
	}

	/**
	 * Full host reset
	 */
	bool reset_all(Delayer & delayer)
	{
		/* start reset */
		write<Sysctl::Rsta>(1);

		/*
		 * The SDHC specification says that a software reset shouldn't
		 * have an effect on the the card detection circuit. The ESDHC
		 * clears Sysctl::Ipgen, Sysctl::Hcken, and Sysctl::Peren
		 * nonetheless which disables clocks that card detection relies
		 * on.
		 */
		Sysctl::access_t sysctl = read<Sysctl>();
		Sysctl::Ipgen::set(sysctl, 1);
		Sysctl::Hcken::set(sysctl, 1);
		Sysctl::Peren::set(sysctl, 1);
		write<Sysctl>(sysctl);

		/* wait for reset completion */
		return wait_for<Sysctl::Rsta>(0, delayer);
	}

	/**
	 * Disable all interrupts
	 */
	void disable_irqs()
	{
		write<Irqstaten>(0);
		write<Irqsigen>(0);
	}

	/**
	 * Enable desired interrupts
	 */
	void enable_irqs()
	{
		Irq::access_t irq = 0;
		Irq::Cc::set(irq, 1);
		Irq::Tc::set(irq, 1);
		Irq::Dint::set(irq, 1);
		Irq::Ctoe::set(irq, 1);
		Irq::Cce::set(irq, 1);
		Irq::Cebe::set(irq, 1);
		Irq::Cie::set(irq, 1);
		Irq::Dtoe::set(irq, 1);
		Irq::Dce::set(irq, 1);
		Irq::Debe::set(irq, 1);
		Irq::Ac12e::set(irq, 1);
		Irq::Dmae::set(irq, 1);
		write<Irqstaten>(irq);
		write<Irqsigen>(irq);
	}

	enum Bus_width { BUS_WIDTH_1, BUS_WIDTH_4 };

	/**
	 * Set bus width
	 */
	void bus_width(Bus_width bus_width)
	{
		switch (bus_width) {
		case BUS_WIDTH_1: write<Proctl::Dtw>(Proctl::Dtw::_1BIT); break;
		case BUS_WIDTH_4: write<Proctl::Dtw>(Proctl::Dtw::_4BIT); break;
		}
	}

	/**
	 * Disable host clock
	 */
	void disable_clock()
	{
		Sysctl::access_t sysctl = read<Sysctl>();
		Sysctl::Ipgen::set(sysctl, 0);
		Sysctl::Hcken::set(sysctl, 0);
		Sysctl::Peren::set(sysctl, 0);
		Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV1);
		Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV1);
		write<Sysctl>(sysctl);
	}

	enum Clock_divider { CLOCK_DIV_8, CLOCK_DIV_512 };

	/**
	 * Enable host clock and configure it to fullfill a given divider
	 */
	void enable_clock(Clock_divider divider, Delayer &delayer)
	{
		Sysctl::access_t sysctl = read<Sysctl>();
		Sysctl::Ipgen::set(sysctl, 1);
		Sysctl::Hcken::set(sysctl, 1);
		Sysctl::Peren::set(sysctl, 1);
		switch (divider) {
		case CLOCK_DIV_8:
			Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV4);
			Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV2);
			break;
		case CLOCK_DIV_512:
			Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV16);
			Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV32);
			break;
		}
		write<Sysctl>(sysctl);
		delayer.usleep(1000);
	}

	/**
	 * Set clock to fulfill a given divider
	 */
	void clock(enum Clock_divider divider, Delayer &delayer)
	{
		disable_clock();
		write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_27);
		enable_clock(divider, delayer);
	}
};

struct Esdhcv2_controller : private Esdhcv2, public Sd_card::Host_controller
{
	private:

		enum {
			BLOCK_SIZE      = 512,
			WATERMARK_WORDS = 16,
			BURST_WORDS     = 8,
		};

		Genode::Irq_connection  _irq;
		Genode::Signal_receiver _irq_rec;
		Genode::Signal_context  _irq_ctx;
		Delayer &               _delayer;
		Sd_card::Card_info      _card_info;
		bool const              _use_dma;
		Adma2::Table            _adma2_table;

		/**
		 * Print 'err' and throw detection-error exception
		 */
		void _detect_err(char const * const err)
		{
			PERR("%s", err);
			throw Detection_failed();
		}

		/**
		 * Initialize host and card and return basic card information
		 */
		Sd_card::Card_info _init()
		{
			/* configure host for initialization stage */
			using namespace Sd_card;
			if (!reset_all(_delayer)) { _detect_err("Host reset failed"); }
			disable_irqs();

			/* check host version */
			Hostver::access_t const hostver = read<Hostver>();
			if (Hostver::Vvn::get(hostver) != 18) {
				_detect_err("Unexpected Vendor Version Number"); }
			if (Hostver::Svn::get(hostver) != 1) {
				_detect_err("Unexpected Specification Version Number"); }

			/*
			 * We should check host capabilities at this point if we want to
			 * support other versions of the ESDHC. For the i.MX53 ESDHCv2 we
			 * know that the capabilities fit our requirements.
			 */

			enable_irqs();
			bus_width(BUS_WIDTH_1);
			_delayer.usleep(10000);
			clock(CLOCK_DIV_512, _delayer);

			/*
			 * Initialize card
			 */

			/*
			 * At this point we should do an SDIO card reset if we later want
			 * to detect the unwanted case of an SDIO card beeing inserted.
			 * The reset would be done via 2 differently configured
			 * Io_rw_direct commands.
			 */

			_delayer.usleep(1000);
			if (!issue_command(Go_idle_state())) {
				_detect_err("Go_idle_state command failed"); }

			_delayer.usleep(2000);
			if (!issue_command(Send_if_cond())) {
				_detect_err("Send_if_cond command failed"); }

			if (read<Cmdrsp0>() != 0x1aa) {
				_detect_err("Unexpected response of Send_if_cond command"); }

			/*
			 * At this point we could detect the unwanted case of an SDIO card
			 * beeing inserted by issuing 4 Io_send_op_cond commands at an
			 * interval of 10 ms (they should time out on SD).
			 */

			if (!issue_command(Sd_send_op_cond(0, false))) {
				_detect_err("Sd_send_op_cond command failed"); }

			if (read<Cmdrsp0>() != 0xff8000) {
				_detect_err("Unexpected response of Sd_send_op_cond command"); }

			_delayer.usleep(1000);
			if (!issue_command(Go_idle_state())) {
				_detect_err("Go_idle_state command failed"); }

			_delayer.usleep(2000);
			if (!issue_command(Send_if_cond())) {
				_detect_err("Send_if_cond failed"); }

			if (read<Cmdrsp0>() != 0x1aa) {
				_detect_err("Unexpected response of Send_if_cond command"); }

			/*
			 * Power on card
			 *
			 * We need to issue the same Sd_send_op_cond command multiple
			 * times. The first time, we receive the status information. On
			 * subsequent attempts, the response tells us that the card is
			 * busy. Usually, the command is issued twice. We give up if the
			 * card is not reaching busy state after one second.
			 */
			int i = 1000;
			for (; i > 0; --i) {
				if (!issue_command(Sd_send_op_cond(0x200000, true))) {
					_detect_err("Sd_send_op_cond command failed"); }

				if (Ocr::Busy::get(read<Cmdrsp0>())) { break; }
				_delayer.usleep(1000);
			}
			if (!i) { _detect_err("Could not power-on SD card"); }

			/* get basic information about the card */
			Card_info card_info = _detect();

			/*
			 * Configure working clock of host
			 *
			 * FIXME Host and card may be driven with a higher clock rate but
			 * checks (maybe read SSR/SCR, read switch, try frequencies) are
			 * necessary for that.
			 */
			clock(CLOCK_DIV_8, _delayer);

			/*
			 * Configure card and host to use 4 data signals
			 *
			 * FIXME Host and card may be driven with a higher bus width but
			 * further checks (read SCR) are necessary for that.
			 */
			if (!issue_command(
					Set_bus_width(Set_bus_width::Arg::Bus_width::FOUR_BITS),
					card_info.rca()))
			{ _detect_err("Set_bus_width(FOUR_BITS) command failed"); }

			bus_width(BUS_WIDTH_4);
			_delayer.usleep(10000);

			/* configure card to use our block size */
			if (!issue_command(Set_blocklen(BLOCK_SIZE))) {
				_detect_err("Set_blocklen command failed"); }

			/* configure host buffer */
			Wml::access_t wml = read<Wml>();
			Wml::Rd_wml::set(wml, WATERMARK_WORDS);
			Wml::Rd_brst_len::set(wml, BURST_WORDS);
			Wml::Wr_wml::set(wml, WATERMARK_WORDS);
			Wml::Wr_brst_len::set(wml, BURST_WORDS);
			write<Wml>(wml);

			/* configure ADMA */
			write<Proctl::Dmas>(Proctl::Dmas::ADMA2);

			/* configure interrupts for operational mode */
			disable_irqs();
			write<Irqstat>(~0);
			enable_irqs();
			return card_info;
		}

		/**
		 * Wait until we received the IRQ signal
		 */
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

		/*
		 * Wait till sending a new command is allowed
		 *
		 * \return true on success
		 */
		bool _wait_for_cmd_allowed()
		{
			/*
			 * At least after multi-block writes with our
			 * "Broken Auto Command 12" fix, waiting only for Prsstat::Cihb
			 * isn't sufficient as Prsstat::Dla and Prsstat::Cdihb may also
			 * be active.
			 */
			if (!wait_for<Prsstat_lhw>(Prsstat_lhw::cmd_allowed(), _delayer)) {
				PERR("wait till issuing a new command is allowed timed out");
				return false;
			}
			return true;
		}

		/**
		 * Wait for the completion of a non-multiple-block command
		 *
		 * \return true on success
		 */
		bool _wait_for_cmd_complete()
		{
			_wait_for_irq();
			if (read<Irqstat>() != Irqstat::Cc::reg_mask()) {
				PWRN("received unexpected host signal");
				reset_command(_delayer);
				reset_data(_delayer);
				enable_irqs();
				return false;
			}
			write<Irqstat>(Irqstat::Cc::reg_mask());
			return true;
		}

		/**
		 * Abort transmission by manually issuing stop command
		 *
		 * \return true on success
		 */
		bool _abort_transmission()
		{
			write<Cmdarg>(0);
			Xfertyp::access_t xfertyp = 0;
			Xfertyp::Cmdinx::set(xfertyp, Sd_card::Stop_transmission::INDEX);
			Xfertyp::Cmdtyp::set(xfertyp, Xfertyp::Cmdtyp::ABORT_CMD12);
			Xfertyp::Rsptyp::set(xfertyp, Xfertyp::Rsptyp::_48BIT);
			write<Xfertyp>(xfertyp);
			return _wait_for_cmd_complete();
		}

		/**
		 * Wait for the completion of a multiple-block command
		 *
		 * \param r  wether the transfer reads from card to host
		 *
		 * \return true on success
		 */
		bool _wait_for_mbc_complete(bool const r)
		{
			/* wait for a signal */
			_wait_for_irq();
			Irqstat::access_t const irq = read<Irqstat>();

			/*
			 * The ESDHC signals on multi-block transfers seem to be broken.
			 * Synchronizing to Irqstat::Tc before returning from transfer
			 * requests and to Prsstat::Cihb before sending the next command,
			 * as it is done with other SDHCs, isn't sufficient. Instead, both
			 * completion signals must be gathered while dealing with the fact
			 * that the interrupt triggers only on signal edges and we can
			 * therefore not wait for a second IRQ when we've received a
			 * single signal the first time.
			 */
			Irqstat::access_t constexpr irq_cc_tc =
				Irq::Cc::reg_mask() | Irq::Tc::reg_mask();
			switch (irq) {
			case Irq::Cc::reg_mask():
			case Irq::Tc::reg_mask():

				/* poll for the missing signal */
				if (!wait_for<Irqstat>(irq_cc_tc, _delayer)) {
					PERR("completion host signal timed out");
					return false;
				}
			case irq_cc_tc:

				/* acknowledge both completion signals */
				write<Irqstat>(irq_cc_tc);

				/*
				 * The "Auto Command 12" feature of the ESDHC seems to be
				 * broken for multi-block writes as it causes command-
				 * timeout errors sometimes. Thus, we end such transfers
				 * manually.
				 */
				return r ? true : _abort_transmission();
			default:
				PERR("received unexpected host signal");
				return false;
			}
		}

		/**
		 * Prepare multi-block-transfer command with DMA
		 *
		 * \param blk_cnt   number of transferred blocks block
		 * \param buf_phys  physical base of transfer buffer
		 *
		 * \return true on success
		 */
		bool _prepare_dma_mbc(size_t blk_cnt, addr_t buf_phys)
		{
			size_t const req_size = blk_cnt * BLOCK_SIZE;
			if (!_adma2_table.setup_req(req_size, buf_phys)) {
				PERR("Setup ADMA2 table failed");
				return false;
			}
			write<Adsaddr>(_adma2_table.base_phys());
			write<Blkattr::Blksize>(BLOCK_SIZE);
			write<Blkattr::Blkcnt>(blk_cnt);
			return true;
		}


		/****************************************
		 ** Sd_card::Host_controller interface **
		 ****************************************/

		Sd_card::Cid _read_cid()
		{
			Sd_card::Cid cid;
			cid.raw_0 = read<Rsp136_0>();
			cid.raw_1 = read<Rsp136_1>();
			cid.raw_2 = read<Rsp136_2>();
			cid.raw_3 = read<Rsp136_3>();
			return cid;
		}

		Sd_card::Csd _read_csd()
		{
			Sd_card::Csd csd;
			csd.csd0 = read<Rsp136_0>();
			csd.csd1 = read<Rsp136_1>();
			csd.csd2 = read<Rsp136_2>();
			csd.csd3 = read<Rsp136_3>();
			return csd;
		}

		unsigned _read_rca()
		{
			Cmdrsp0::access_t const rsp0 = read<Cmdrsp0>();
			return Sd_card::Send_relative_addr::Response::Rca::get(rsp0);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param mmio_base  local base address of MMIO registers
		 * \param irq        host-interrupt ID
		 * \param delayer    delayer timing of MMIO accesses
		 * \param use_dma    wether to use DMA or direct IO for transfers
		 */
		Esdhcv2_controller(addr_t const mmio_base, unsigned const irq,
		                   Delayer & delayer, bool const use_dma)
		:
			Esdhcv2(mmio_base), _irq(irq), 
			_delayer(delayer), _card_info(_init()), _use_dma(use_dma)
		{
			_irq.sigh(_irq_rec.manage(&_irq_ctx));
		}

		~Esdhcv2_controller() { _irq_rec.dissolve(&_irq_ctx); }


		/****************************************
		 ** Sd_card::Host_controller interface **
		 ****************************************/

		bool _issue_command(Sd_card::Command_base const & command)
		{
			/* detect if command is a multi-block transfer and if it reads */
			using namespace Sd_card;
			bool const r = command.transfer == Sd_card::TRANSFER_READ;
			bool const mb =
				command.index == Sd_card::Read_multiple_block::INDEX ||
				command.index == Sd_card::Write_multiple_block::INDEX;

			/* assemble comsendcode */
			Xfertyp::access_t cmd = 0;
			Xfertyp::Cmdinx::set(cmd, command.index);
			if (command.transfer != Sd_card::TRANSFER_NONE) {
				Xfertyp::Dpsel::set(cmd);
				Xfertyp::Bcen::set(cmd);
				Xfertyp::Msbsel::set(cmd);
				if (mb) {
					/*
					 * The "Auto Command 12" feature of the ESDHC seems to be
					 * broken for multi-block writes as it causes command-
					 * timeout errors sometimes. Thus, we end such transfers
					 * manually.
					 */
					if (r) { Xfertyp::Ac12en::set(cmd); }
					if (_use_dma) { Xfertyp::Dmaen::set(cmd); }
				}
				Xfertyp::Dtdsel::set(cmd,
					r ? Xfertyp::Dtdsel::READ : Xfertyp::Dtdsel::WRITE);
			}
			typedef Xfertyp::Rsptyp Rsptyp;
			Xfertyp::access_t rt = 0;
			switch (command.rsp_type) {
			case RESPONSE_NONE:             rt = Rsptyp::_0BIT;       break;
			case RESPONSE_136_BIT:          rt = Rsptyp::_136BIT;     break;
			case RESPONSE_48_BIT:           rt = Rsptyp::_48BIT;      break;
			case RESPONSE_48_BIT_WITH_BUSY: rt = Rsptyp::_48BIT_BUSY; break;
			}
			Xfertyp::Rsptyp::set(cmd, rt);

			/* send command as soon as the host allows it */
			if (!_wait_for_cmd_allowed()) { return false; }
			write<Cmdarg>(command.arg);
			write<Xfertyp>(cmd);

			/* wait for completion */
			return mb ? _wait_for_mbc_complete(r) : _wait_for_cmd_complete();
		}

		Sd_card::Card_info card_info() const { return _card_info; }

		bool read_blocks(size_t const block_number, size_t const block_count,
		                 char * out_buffer_phys)
		{
			PERR("block transfer without DMA not supported by now");
			return false;
		}

		bool write_blocks(size_t block_number, size_t block_count,
		                  char const *buffer_phys)
		{
			PERR("block transfer without DMA not supported by now");
			return false;
		}

		bool read_blocks_dma(size_t blk_nr, size_t blk_cnt, addr_t buf_phys)
		{
			if (!_prepare_dma_mbc(blk_cnt, buf_phys)) { return false; }
			return issue_command(Sd_card::Read_multiple_block(blk_nr));
		}

		bool write_blocks_dma(size_t blk_nr, size_t blk_cnt, addr_t buf_phys)
		{
			if (!_prepare_dma_mbc(blk_cnt, buf_phys)) { return false; }
			return issue_command(Sd_card::Write_multiple_block(blk_nr));
		}
};

#endif /* _ESDHCV2_H_ */
