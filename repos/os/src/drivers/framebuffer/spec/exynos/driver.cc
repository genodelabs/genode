/*
 * \brief  Framebuffer driver for Exynos5 HDMI
 * \author Martin Stein
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <driver.h>

/* Genode includes */
#include <timer_session/connection.h>
#include <irq_session/connection.h>
#include <regulator_session/connection.h>
#include <os/attached_mmio.h>
#include <drivers/board_base.h>

using namespace Genode;

/**
 * Delayer with timer backend
 */
class Timer_delayer : public Mmio::Delayer, public Timer::Connection
{
	public:

		/*************
		 ** Delayer **
		 *************/

		void usleep(unsigned us) { Timer::Connection::usleep(us); }
};

/**
 * Singleton delayer for MMIO polling
 */
static Mmio::Delayer * delayer()
{
	static Timer_delayer s;
	return &s;
}

/**
 * Singleton regulator for HDMI clocks
 */
static Regulator::Connection * hdmi_clock()
{
	static Regulator::Connection s(Regulator::CLK_HDMI);
	return &s;
}

/**
 * Singleton regulator for HDMI clocks
 */
static Regulator::Connection * hdmi_power()
{
	static Regulator::Connection s(Regulator::PWR_HDMI);
	return &s;
}

/**
 * Sends and receives data via I2C protocol as master or slave
 */
class I2c_interface : public Attached_mmio
{
	private:

		/********************
		 ** MMIO structure **
		 ********************/

		struct Start_msg : Genode::Register<8>
		{
			struct Rx   : Bitfield<0, 1> { };
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
			struct Slave_addr : Bitfield<1, 7> { };
		};
		struct Ds  : Register<0xc, 8> { };
		struct Lc  : Register<0x10, 8>
		{
			struct Sda_out_delay : Bitfield<0, 2> { };
			struct Filter_en     : Bitfield<2, 1> { };
		};

		enum {
			MASTER_RX   = 2,
			MASTER_TX   = 3,
			TX_DELAY_US = 1,
		};

		Irq_connection          _irq;
		Genode::Signal_receiver _irq_rec;
		Genode::Signal_context  _irq_ctx;

		/**
		 * Wait until the IRQ signal was received
		 */
		void _wait_for_irq()
		{
			_irq_rec.wait_for_signal();

			_irq.ack_irq();
		}

		/**
		 * Stop a running transfer as master
		 */
		void _stop_m_transfer()
		{
			write<Con::Irq_en>(0);
			write<Stat::Busy>(0);
			write<Con::Irq_pending>(0);
			if (read<Stat::Busy>()) {
				warning("I2C got stuck after transfer, forcely terminate");
				write<Stat::Txrx_en>(0);
			}
		}

		/**
		 * Start transfer of a message as master
		 *
		 * \param slave  I2C address of targeted slave
		 * \param tx     wether to send or receive
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _start_m_transfer(uint8_t slave, bool tx)
		{
			/* create start op and get bus */
			Start_msg::access_t start = 0;
			Start_msg::Addr::set(start, slave);
			Start_msg::Rx::set(start, !tx);
			if (!wait_for<Stat::Busy>(0, *delayer())) {
				error("I2C to busy to do transfer");
				return -1;
			}
			/* enable signal receipt */
			Con::access_t con = read<Con>();
			Con::Irq_en::set(con, 1);
			Con::Ack_en::set(con, 1);
			write<Con>(con);

			/* send start op */
			Stat::access_t stat = 0;
			Stat::Txrx_en::set(stat, 1);
			Stat::Mode::set(stat, tx ? MASTER_TX : MASTER_RX);
			write<Stat>(stat);
			write<Ds>(start);
			delayer()->usleep(TX_DELAY_US);

			/* end start-op transfer */
			write<Con>(con);
			Stat::Busy::set(stat, 1);
			write<Stat>(stat);
			_wait_for_irq();
			if (_arbitration_error()) return -1;
			return 0;
		}

		/**
		 * Wether acknowledgment for last transaction can be received
		 */
		bool _ack_received()
		{
			for (unsigned i = 0; i < 3; i++) {
				if (read<Con::Irq_pending>() && !read<Stat::Last_bit>()) return 1;
				delayer()->usleep(TX_DELAY_US);
			}
			error("I2C ack not received");
			return 0;
		}

		/**
		 * Wether arbitration errors occured during the last transaction
		 */
		bool _arbitration_error()
		{
			if (read<Stat::Arbitr>()) {
				error("I2C arbitration failed");
				return 1;
			}
			return 0;
		}


	public:

		/**
		 * Constructor
		 *
		 * \param base  physical MMIO base
		 * \param irq   interrupt name
		 */
		I2c_interface(addr_t base, unsigned irq)
		:
			Attached_mmio(base, 0x10000), _irq(irq)
		{
			/* FIXME: is this a correct slave address? */
			write<Add::Slave_addr>(0);

			/* FIXME: do prescaler/clk generic (Lx: s3c24xx_i2c_clockrate) */
			Con::access_t con = 0;
			Con::Irq_en::set(con, 1);
			Con::Ack_en::set(con, 1);
			Con::Tx_prescaler::set(con, 1);
			Con::Clk_sel::set(con, 1);
			write<Con>(con);

			/* FIXME: do delay/filter generic (Lx: s3c24xx_i2c_clockrate) */
			Lc::access_t lc = 0;
			Lc::Sda_out_delay::set(lc, 2);
			Lc::Filter_en::set(lc, 1);
			write<Lc>(lc);

			_irq.sigh(_irq_rec.manage(&_irq_ctx));
			_irq.ack_irq();
		}

		~I2c_interface() { _irq_rec.dissolve(&_irq_ctx); }

		/**
		 * Transmit an I2C message as master
		 *
		 * \param slave     I2C address of targeted slave
		 * \param msg       base of message
		 * \param msg_size  size of message
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int m_transmit(uint8_t slave, uint8_t * msg, size_t msg_size)
		{
			/* initialize message transfer */
			if (_start_m_transfer(slave, 1)) return -1;
			size_t off = 0;
			while (1)
			{
				/* send next message byte or leave if message end reached */
				if (!_ack_received()) return -1;
				if (off == msg_size) break;
				write<Ds>(msg[off]);
				delayer()->usleep(TX_DELAY_US);

				/* finish last byte and prepare for next one */
				off++;
				write<Con::Irq_pending>(0);
				_wait_for_irq();
				if (_arbitration_error()) return -1;
			}
			/* end message transfer */
			if (!_ack_received()) return -1;
			_stop_m_transfer();
			return 0;
		}

		/**
		 * Receive an I2C message as master
		 *
		 * \param slave     I2C address of targeted slave
		 * \param buf       base of receive buffer
		 * \param buf_size  size of receive buffer (= transfer size)
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int m_receive(uint8_t slave, uint8_t * buf, size_t buf_size)
		{
			/* check receive buffer and initialize message transfer */
			if (!buf_size) {
				error("zero-sized receive buffer");
				return -1;
			}
			if (_start_m_transfer(slave, 0)) return -1;
			write<Con::Irq_pending>(0);
			size_t off     = 0;
			bool last_byte = 0;
			while (1)
			{
				/* receive next message byte */
				_wait_for_irq();
				if (_arbitration_error()) return -1;
				buf[off] = read<Ds>();
				off++;

				/* acknowledge receipt or leave if buffer is full */
				if (last_byte) break;
				if (off == buf_size - 1) {
					write<Con::Ack_en>(0);
					last_byte = 1;
				}
				write<Con::Irq_pending>(0);
			}
			/* end message transfer */
			_stop_m_transfer();
			return 0;
		}
};

/**
 * Mixes several video and graphic inputs to get a single output stream
 */
class Video_mixer : public Attached_mmio
{
	private:

		/********************
		 ** MMIO structure **
		 ********************/

		struct Status : Register<0x0, 32>
		{
			struct Reg_run      : Bitfield<0, 1> { };
			struct Sync_enable  : Bitfield<2, 1> { };
			struct Dma_16_burst : Bitfield<7, 1> { };
			struct Soft_reset   : Bitfield<8, 1> { };
		};
		struct Cfg : Register<0x4, 32>
		{
			struct Hd_sd        : Bitfield<0, 1> { };
			struct Scan_mode    : Bitfield<2, 1> { };
			struct M0_video_en  : Bitfield<3, 1> { };
			struct M0_g0_en     : Bitfield<4, 1> { };
			struct M0_g1_en     : Bitfield<5, 1> { };
			struct Dst_sel      : Bitfield<7, 1> { };
			struct Hd_mode      : Bitfield<6, 1> { };
			struct Out_type     : Bitfield<8, 1> { };
			struct Rgb_format   : Bitfield<9, 2> { };
			struct M1_video_en  : Bitfield<13, 1> { };
			struct M1_g0_en     : Bitfield<14, 1> { };
			struct M1_g1_en     : Bitfield<15, 1> { };
			struct Layer_update : Bitfield<31, 1> { };
		};
		struct Irq_en : Register<0x8, 32> { };

		template <unsigned OFF>
		struct Grp_cfg : Register<OFF, 32>
		{
			struct Color_format   : Register<OFF, 32>::template Bitfield<8, 4> { };
			struct Pixel_blend_en : Register<OFF, 32>::template Bitfield<16, 1> { };
			struct Win_blend_en   : Register<OFF, 32>::template Bitfield<17, 1> { };
			struct Pre_mul_mode   : Register<OFF, 32>::template Bitfield<20, 1> { };
			struct Blank_change   : Register<OFF, 32>::template Bitfield<21, 1> { };
			struct Rtqos          : Register<OFF, 32>::template Bitfield<23, 9> { };
		};
		struct M0_g0_cfg  : Grp_cfg<0x20> { };
		struct M0_g0_base : Register<0x24, 32> { };

		template <unsigned OFF>
		struct Bg_color : Register<OFF, 32>
		{
			struct Ycbcr : Register<OFF, 32>::template Bitfield<0, 24> { };
		};
		struct M0_bg_color0 : Bg_color<0x64> { };
		struct M0_bg_color1 : Bg_color<0x68> { };
		struct M0_bg_color2 : Bg_color<0x6c> { };

		struct M0_layer_cfg : Register<0x10, 32>
		{
			struct Video_prio : Bitfield<0, 4> { };
			struct G0_prio    : Bitfield<4, 4> { };
			struct G1_prio    : Bitfield<8, 4> { };
		};
		struct M0_g0_span : Register<0x28, 32>
		{
			struct Span : Bitfield<0, 15> { };
		};
		template <unsigned OFF>
		struct Xy : Register<OFF, 32>
		{
			struct Y : Register<OFF, 32>::template Bitfield<0, 11> { };
			struct X : Register<OFF, 32>::template Bitfield<16, 11> { };
		};
		struct M0_g0_sxy : Xy<0x2c> { };
		struct M0_g0_dxy : Xy<0x34> { };

		struct M0_g0_wh : Register<0x30, 32>
		{
			struct Height  : Bitfield<0, 11> { };
			struct V_scale : Bitfield<12, 2> { };
			struct Width   : Bitfield<16, 11> { };
			struct H_scale : Bitfield<28, 2> { };
		};
		struct M0_cm_coeff_y  : Register<0x80, 32> { };
		struct M0_cm_coeff_cb : Register<0x84, 32> { };
		struct M0_cm_coeff_cr : Register<0x88, 32> { };

	public:

		typedef Framebuffer::Driver::Format Format;

		/**
		 * Constructor
		 */
		Video_mixer() : Attached_mmio(Genode::Board_base::MIXER_BASE, 0x10000) { }

		/**
		 * Initialize mixer for displaying one graphical input fullscreen
		 *
		 * \param fb_phys    physical base of framebuffer
		 * \param fb_width   width of framebuffer in pixel
		 * \param fb_height  height of framebuffer in pixel
		 * \param fb_format  pixel format of framebuffer
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int init_mxr(addr_t const fb_phys, size_t const fb_width,
		             size_t const fb_height, Format const fb_format)
		{
			using namespace Framebuffer;

			/* reset and disable */
			write<Status::Soft_reset>(1);
			write<Irq_en>(0);
			write<Status::Sync_enable>(0);

			/* global layer switches and output config */
			Cfg::access_t cfg = read<Cfg>();
			Cfg::M0_video_en::set(cfg, 0);
			Cfg::M0_g0_en::set(cfg, 0);
			Cfg::M0_g1_en::set(cfg, 0);
			Cfg::Dst_sel::set(cfg, 1); /* HDMI */
			Cfg::Out_type::set(cfg, 1); /* RGB888 */
			Cfg::M1_video_en::set(cfg, 0);
			Cfg::M1_g0_en::set(cfg, 0);
			Cfg::M1_g1_en::set(cfg, 0);
			write<Cfg>(cfg);

			/* global input config */
			write<Status::Dma_16_burst>(1);

			/* layer arrangement of mixer 0 */
			M0_layer_cfg::access_t lcfg = read<M0_layer_cfg>();
			M0_layer_cfg::Video_prio::set(lcfg, 0); /* hidden */
			M0_layer_cfg::G0_prio::set(lcfg, 1); /* framebuffer */
			M0_layer_cfg::G1_prio::set(lcfg, 0); /* hidden */
			write<M0_layer_cfg>(lcfg);

			/* background colors of mixer 0 */
			enum { BLACK = 0x8080 };
			write<M0_bg_color0::Ycbcr>(BLACK);
			write<M0_bg_color1::Ycbcr>(BLACK);
			write<M0_bg_color2::Ycbcr>(BLACK);

			/* common config of graphic input 0 of mixer 0 */
			M0_g0_cfg::access_t gcfg = read<M0_g0_cfg>();
			M0_g0_cfg::Rtqos::set(gcfg, 0);
			M0_g0_cfg::Pre_mul_mode::set(gcfg, 0);
			M0_g0_cfg::Blank_change::set(gcfg, 1); /* no blank key */
			M0_g0_cfg::Win_blend_en::set(gcfg, 0);
			M0_g0_cfg::Pixel_blend_en::set(gcfg, 0);
			write<M0_g0_cfg>(gcfg);

			/* input pixel format */
			switch (fb_format) {
			case Driver::FORMAT_RGB565:
				write<M0_g0_cfg::Color_format>(4);
				break;
			default:
				error("framebuffer format not supported");
				return -1;
			}
			/* window measurements */
			write<M0_g0_span::Span>(fb_width);
			M0_g0_wh::access_t wh = read<M0_g0_wh>();
			M0_g0_wh::Height::set (wh, fb_height);
			M0_g0_wh::V_scale::set(wh, 0); /* don't scale */
			M0_g0_wh::Width::set  (wh, fb_width);
			M0_g0_wh::H_scale::set(wh, 0); /* don't scale */
			write<M0_g0_wh>(wh);

			/* window location at input */
			M0_g0_sxy::access_t sxy = read<M0_g0_sxy>();
			M0_g0_sxy::Y::set(sxy, 0);
			M0_g0_sxy::X::set(sxy, 0);
			write<M0_g0_sxy>(sxy);

			/* window location at output */
			M0_g0_dxy::access_t dxy = read<M0_g0_dxy>();
			M0_g0_dxy::Y::set(dxy, 0);
			M0_g0_dxy::X::set(dxy, 0);
			write<M0_g0_dxy>(dxy);

			/* set-up input DMA */
			write<M0_g0_base>(fb_phys);

			/*
			 * FIXME: For FB heights greater than 576 Linaro uses RGB709 16-235,
			 *        wich implies reconfiguration of regs 0x80, 0x84, and 0x88.
			 *        As we always use RGB601 0-255 we can live with reset values.
			 */

			/* adjust global output config and enable layer */
			cfg = read<Cfg>();
			switch (fb_height) {
			case 480:
				Cfg::Hd_sd::set(cfg, 1);
				Cfg::Hd_mode::set(cfg, 1);
				break;
			case 576:
				Cfg::Hd_sd::set(cfg, 1);
				Cfg::Hd_mode::set(cfg, 1);
				break;
			case 720:
				Cfg::Hd_sd::set(cfg, 1);
				Cfg::Hd_mode::set(cfg, 1);
				break;
			case 1080:
				Cfg::Hd_sd::set(cfg, 1);
				Cfg::Hd_mode::set(cfg, 1);
				break;
			default:
				error("framebuffer height not supported");
				return -1;
			}
			Cfg::Scan_mode::set(cfg, 1); /* progressive */
			Cfg::M0_g0_en::set(cfg, 1);
			Cfg::Rgb_format::set(cfg, 0); /* RGB601, 0-255 */
			Cfg::Layer_update::set(cfg, 1);
			write<Cfg>(cfg);

			/* start mixer */
			write<Status::Reg_run>(1);
			write<Status::Sync_enable>(1);
			return 0;
		}
};

/**
 * Return singleton of device instance
 */
static Video_mixer * video_mixer()
{
	static Video_mixer s;
	return &s;
}

/**
 * Dedicated I2C interface for communicating with HDMI PHY controller
 */
class I2c_hdmi : public I2c_interface
{
	private:

		enum {
			SLAVE_ADDR     = 0x08,
			HDMI_PHY_SLAVE = 0x38,
		};

	public:

		/**
		 * Constructor
		 */
		I2c_hdmi()
		: I2c_interface(Genode::Board_base::I2C_BASE, Genode::Board_base::I2C_HDMI_IRQ) { }

		/**
		 * Stop HDMI PHY from operating
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int stop_hdmi_phy()
		{
			uint8_t stop[] = { 0x1f, 0x00 };
			enum { STOP_SIZE = sizeof(stop)/sizeof(stop[0])  };
			if (m_transmit(HDMI_PHY_SLAVE, stop, STOP_SIZE)) { return -1; }
			return 0;
		}

		/**
		 * Configure HDMI PHY for the given parameters and start it
		 *
		 * \param pixel_clk  frequency of pixel processing
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int setup_and_start_hdmi_phy(unsigned const pixel_clk)
		{
			static uint8_t cfg_148_5[] = { 0x01,
				0xd1,0x1f,0x00,0x40,0x40,
				0xf8,0x08,0x81,0xa0,0xba,
				0xd8,0x45,0xa0,0xac,0x80,
				0x3c,0x80,0x11,0x04,0x02,
				0x22,0x44,0x86,0x54,0x4b,
				0x25,0x03,0x00,0x00,0x01,
				0x00 };

			/* select and write config */
			uint8_t * cfg;
			size_t    cfg_size;
			switch (pixel_clk) {
			case 148500000:
				cfg      = cfg_148_5;
				cfg_size = sizeof(cfg_148_5)/sizeof(cfg_148_5[0]);
				break;
			default:
				error("pixel clock not supported");
				return -1;
			}
			if (m_transmit(HDMI_PHY_SLAVE, cfg, cfg_size)) { return -1; }

			/* ensure that configuration is applied */
			delayer()->usleep(10000);

			/* start hdmi phy */
			static uint8_t start[] = { 0x1f, 0x80 };
			enum { START_SIZE = sizeof(start)/sizeof(start[0])  };
			if (m_transmit(HDMI_PHY_SLAVE, start, START_SIZE)) { return -1; }
			return 0;
		}
};


/**
 * Converts input stream from video mixer into HDMI packet stream for HDMI PHY
 */
class Hdmi : public Attached_mmio
{
	private:

		/**********************************
		 ** Utilities for MMIO structure **
		 **********************************/

		template <unsigned OFF>
		struct B13o8_0 : Register<OFF, 8>
		{
			struct Bits_0_8 : Register<OFF, 8>::template Bitfield<0, 8> { };
		};
		template <unsigned OFF>
		struct B13o8_1 : Register<OFF, 8>
		{
			struct Bits_8_5 : Register<OFF, 8>::template Bitfield<0, 5> { };
		};
		template <unsigned OFF>
		struct B12o8_0 : Register<OFF, 8>
		{
			struct Bits_0_8 : Register<OFF, 8>::template Bitfield<0, 8> { };
		};
		template <unsigned OFF>
		struct B12o8_1 : Register<OFF, 8>
		{
			struct Bits_8_4 : Register<OFF, 8>::template Bitfield<0, 5> { };
		};
		template <unsigned OFF>
		struct B11o8_0 : Register<OFF, 8>
		{
			struct Bits_0_8 : Register<OFF, 8>::template Bitfield<0, 8> { };
		};
		template <unsigned OFF>
		struct B11o8_1 : Register<OFF, 8>
		{
			struct Bits_8_3 : Register<OFF, 8>::template Bitfield<0, 3> { };
		};

		template <typename REG_0, typename REG_1>
		void b13o8(uint16_t v)
		{
			write<typename REG_0::Bits_0_8>(v);
			write<typename REG_1::Bits_8_5>(v >> 8);
		}

		template <typename REG_0, typename REG_1>
		void b12o8(uint16_t v)
		{
			write<typename REG_0::Bits_0_8>(v);
			write<typename REG_1::Bits_8_4>(v >> 8);
		}

		template <typename REG_0, typename REG_1>
		void b11o8(uint16_t v)
		{
			write<typename REG_0::Bits_0_8>(v);
			write<typename REG_1::Bits_8_3>(v >> 8);
		}


		/**************************************************
		 ** MMIO structure: Control registers 0x1453xxxx **
		 **************************************************/

		struct Intc_con_0 : Register<0x0, 8>
		{
			struct En_global : Bitfield<6, 1> { };
		};
		struct Phy_status_0 : Register<0x20, 32>
		{
			struct Phy_ready : Bitfield<0, 1> { };
		};
		struct Phy_con_0 : Register<0x30, 8>
		{
			struct Pwr_off : Bitfield<0, 1> { };
		};
		template <unsigned OFF>
		struct Rstout : Register<OFF, 8>
		{
			struct Reset : Register<OFF, 8>::template Bitfield<0, 1> { };
		};
		struct Phy_rstout  : Rstout<0x74> { };
		struct Core_rstout : Rstout<0x80> { };


		/***********************************************
		 ** MMIO structure: Core registers 0x1454xxxx **
		 ***********************************************/

		enum { CORE = 0x10000 };

		template <unsigned OFF>
		struct Sync_pol : Register<OFF, 8>
		{
			struct Pol : Register<OFF, 8>::template Bitfield<0, 1> { };
		};
		struct Con_0 : Register<CORE + 0x0, 8>
		{
			struct System_en   : Bitfield<0, 1> { };
			struct Blue_scr_en : Bitfield<5, 1> { };
		};
		struct Mode_sel : Register<CORE + 0x40, 8>
		{
			struct Mode : Bitfield<0, 2> { };
		};
		struct H_blank  : Bitset_2< B13o8_0<CORE + 0x0a0>,
		                            B13o8_1<CORE + 0x0a4> > { };
		struct V2_blank : Bitset_2< B13o8_0<CORE + 0x0b0>,
		                            B13o8_1<CORE + 0x0b4> > { };
		struct V1_blank : Bitset_2< B13o8_0<CORE + 0x0b8>,
		                            B13o8_1<CORE + 0x0bc> > { };
		struct V_line   : Bitset_2< B13o8_0<CORE + 0x0c0>,
		                            B13o8_1<CORE + 0x0c4> > { };
		struct H_line   : Bitset_2< B13o8_0<CORE + 0x0c8>,
		                            B13o8_1<CORE + 0x0cc> > { };
		struct Hsync_pol         : Sync_pol<CORE + 0x0e0> { };
		struct Vsync_pol         : Sync_pol<CORE + 0x0e4> { };
		struct Int_pro_mode      : Register<CORE + 0x0e8, 8>
		{
			struct Mode : Bitfield<0, 1> { };
		};
		struct V_blank_f0            : Bitset_2< B13o8_0 <CORE + 0x110>,
		                                         B13o8_1 <CORE + 0x114> > { };
		struct V_blank_f1            : Bitset_2< B13o8_0 <CORE + 0x118>,
		                                         B13o8_1 <CORE + 0x11c> > { };
		struct H_sync_start          : Bitset_2< B11o8_0 <CORE + 0x120>,
		                                         B11o8_1 <CORE + 0x124> > { };
		struct H_sync_end            : Bitset_2< B11o8_0 <CORE + 0x128>,
		                                         B11o8_1 <CORE + 0x12c> > { };
		struct V_sync_line_bef_2     : Bitset_2< B13o8_0 <CORE + 0x130>,
		                                         B13o8_1 <CORE + 0x134> > { };
		struct V_sync_line_bef_1     : Bitset_2< B13o8_0 <CORE + 0x138>,
		                                         B13o8_1 <CORE + 0x13c> > { };
		struct V_sync_line_aft_2     : Bitset_2< B13o8_0 <CORE + 0x140>,
		                                         B13o8_1 <CORE + 0x144> > { };
		struct V_sync_line_aft_1     : Bitset_2< B13o8_0 <CORE + 0x148>,
		                                         B13o8_1 <CORE + 0x14c> > { };
		struct V_sync_line_aft_pxl_2 : Bitset_2< B13o8_0 <CORE + 0x150>,
		                                         B13o8_1 <CORE + 0x154> > { };
		struct V_sync_line_aft_pxl_1 : Bitset_2< B13o8_0 <CORE + 0x158>,
		                                         B13o8_1 <CORE + 0x15c> > { };
		struct V_blank_f2            : Bitset_2< B13o8_0 <CORE + 0x160>,
		                                         B13o8_1 <CORE + 0x164> > { };
		struct V_blank_f3            : Bitset_2< B13o8_0 <CORE + 0x168>,
		                                         B13o8_1 <CORE + 0x16c> > { };
		struct V_blank_f4            : Bitset_2< B13o8_0 <CORE + 0x170>,
		                                         B13o8_1 <CORE + 0x174> > { };
		struct V_blank_f5            : Bitset_2< B13o8_0 <CORE + 0x178>,
		                                         B13o8_1 <CORE + 0x17c> > { };
		struct V_sync_line_aft_3     : Bitset_2< B13o8_0 <CORE + 0x180>,
		                                         B13o8_1 <CORE + 0x184> > { };
		struct V_sync_line_aft_4     : Bitset_2< B13o8_0 <CORE + 0x188>,
		                                         B13o8_1 <CORE + 0x18c> > { };
		struct V_sync_line_aft_5     : Bitset_2< B13o8_0 <CORE + 0x190>,
		                                         B13o8_1 <CORE + 0x194> > { };
		struct V_sync_line_aft_6     : Bitset_2< B13o8_0 <CORE + 0x198>,
		                                         B13o8_1 <CORE + 0x19c> > { };
		struct V_sync_line_aft_pxl_3 : Bitset_2< B13o8_0 <CORE + 0x1a0>,
		                                         B13o8_1 <CORE + 0x1a4> > { };
		struct V_sync_line_aft_pxl_4 : Bitset_2< B13o8_0 <CORE + 0x1a8>,
		                                         B13o8_1 <CORE + 0x1ac> > { };
		struct V_sync_line_aft_pxl_5 : Bitset_2< B13o8_0 <CORE + 0x1b0>,
		                                         B13o8_1 <CORE + 0x1b4> > { };
		struct V_sync_line_aft_pxl_6 : Bitset_2< B13o8_0 <CORE + 0x1b8>,
		                                         B13o8_1 <CORE + 0x1bc> > { };
		struct Vact_space_1          : Bitset_2< B13o8_0 <CORE + 0x1c0>,
		                                         B13o8_1 <CORE + 0x1c4> > { };
		struct Vact_space_2          : Bitset_2< B13o8_0 <CORE + 0x1c8>,
		                                         B13o8_1 <CORE + 0x1cc> > { };
		struct Vact_space_3          : Bitset_2< B13o8_0 <CORE + 0x1d0>,
		                                         B13o8_1 <CORE + 0x1d4> > { };
		struct Vact_space_4          : Bitset_2< B13o8_0 <CORE + 0x1d8>,
		                                         B13o8_1 <CORE + 0x1dc> > { };
		struct Vact_space_5          : Bitset_2< B13o8_0 <CORE + 0x1e0>,
		                                         B13o8_1 <CORE + 0x1e4> > { };
		struct Vact_space_6          : Bitset_2< B13o8_0 <CORE + 0x1e8>,
		                                         B13o8_1 <CORE + 0x1ec> > { };
		struct Avi_con                         : Register<CORE + 0x700, 8>
		{
			struct Tx_con : Bitfield<0, 2> { };
		};
		struct Avi_header_0  : Register<CORE + 0x710, 8> { };
		struct Avi_header_1  : Register<CORE + 0x714, 8> { };
		struct Avi_header_2  : Register<CORE + 0x718, 8> { };
		struct Avi_check_sum : Register<CORE + 0x71c, 8> { };
		struct Avi_data_1    : Register<CORE + 0x720, 8> { };
		struct Avi_data_2    : Register<CORE + 0x724, 8> { };
		struct Avi_data_3    : Register<CORE + 0x728, 8> { };
		struct Avi_data_4    : Register<CORE + 0x72c, 8> { };
		struct Avi_data_5    : Register<CORE + 0x730, 8> { };
		struct Avi_data_6    : Register<CORE + 0x734, 8> { };
		struct Avi_data_7    : Register<CORE + 0x738, 8> { };
		struct Avi_data_8    : Register<CORE + 0x73c, 8> { };
		struct Avi_data_9    : Register<CORE + 0x740, 8> { };
		struct Avi_data_10   : Register<CORE + 0x744, 8> { };
		struct Avi_data_11   : Register<CORE + 0x748, 8> { };
		struct Avi_data_12   : Register<CORE + 0x74c, 8> { };
		struct Avi_data_13   : Register<CORE + 0x750, 8> { };


		/***********************************************************
		 ** MMIO structure: Timing-generator registers 0x1458xxxx **
		 ***********************************************************/

		enum { TG = 0x50000 };

		struct Cmd : Register<TG + 0x0, 8>
		{
			struct Tg_en : Bitfield<0, 1> { };
		};
		struct H_fsz          : Bitset_2< B13o8_0 <TG + 0x18>,
		                                  B13o8_1 <TG + 0x1c> > { };
		struct Hact_st        : Bitset_2< B12o8_0 <TG + 0x20>,
		                                  B12o8_1 <TG + 0x24> > { };
		struct Hact_sz        : Bitset_2< B12o8_0 <TG + 0x28>,
		                                  B12o8_1 <TG + 0x2c> > { };
		struct V_fsz          : Bitset_2< B11o8_0 <TG + 0x30>,
		                                  B11o8_1 <TG + 0x34> > { };
		struct Vsync          : Bitset_2< B11o8_0 <TG + 0x38>,
		                                  B11o8_1 <TG + 0x3c> > { };
		struct Vsync2         : Bitset_2< B11o8_0 <TG + 0x40>,
		                                  B11o8_1 <TG + 0x44> > { };
		struct Vact_st        : Bitset_2< B11o8_0 <TG + 0x48>,
		                                  B11o8_1 <TG + 0x4c> > { };
		struct Vact_sz        : Bitset_2< B11o8_0 <TG + 0x50>,
		                                  B11o8_1 <TG + 0x54> > { };
		struct Field_chg      : Bitset_2< B11o8_0 <TG + 0x58>,
		                                  B11o8_1 <TG + 0x5c> > { };
		struct Vact_st2       : Bitset_2< B11o8_0 <TG + 0x60>,
		                                  B11o8_1 <TG + 0x64> > { };
		struct Vact_st3       : Bitset_2< B11o8_0 <TG + 0x68>,
		                                  B11o8_1 <TG + 0x6c> > { };
		struct Vact_st4       : Bitset_2< B11o8_0 <TG + 0x70>,
		                                  B11o8_1 <TG + 0x74> > { };
		struct Vsync_top_hdmi : Bitset_2< B11o8_0 <TG + 0x78>,
		                                  B11o8_1 <TG + 0x7c> > { };
		struct Vsync_bot_hdmi : Bitset_2< B11o8_0 <TG + 0x80>,
		                                  B11o8_1 <TG + 0x84> > { };
		struct Field_top_hdmi : Bitset_2< B11o8_0 <TG + 0x88>,
		                                  B11o8_1 <TG + 0x8c> > { };
		struct Field_bot_hdmi : Bitset_2< B11o8_0 <TG + 0x90>,
		                                  B11o8_1 <TG + 0x94> > { };
		struct Fp_3d                    : Register<TG + 0xf0, 8>
		{
			struct Value : Bitfield<0, 1> { };
		};

		/**
		 * Configure core and timing generator for CEA video mode 16
		 */
		void _setup_mode_16()
		{
			/* core config */
			write<H_blank>(280);
			write<V2_blank>(1125);
			write<V1_blank>(45);
			write<V_line>(1125);
			write<H_line>(2200);
			write<Hsync_pol::Pol>(0);
			write<Vsync_pol::Pol>(0);
			write<Int_pro_mode::Mode>(0);
			write<V_blank_f0>(~0);
			write<V_blank_f1>(~0);
			write<H_sync_start>(86);
			write<H_sync_end>(130);
			write<V_sync_line_bef_2>(9);
			write<V_sync_line_bef_1>(4);
			write<V_sync_line_aft_2>(~0);
			write<V_sync_line_aft_1>(~0);
			write<V_sync_line_aft_pxl_2>(~0);
			write<V_sync_line_aft_pxl_1>(~0);
			write<V_blank_f2>(~0);
			write<V_blank_f3>(~0);
			write<V_blank_f4>(~0);
			write<V_blank_f5>(~0);
			write<V_sync_line_aft_3>(~0);
			write<V_sync_line_aft_4>(~0);
			write<V_sync_line_aft_5>(~0);
			write<V_sync_line_aft_6>(~0);
			write<V_sync_line_aft_pxl_3>(~0);
			write<V_sync_line_aft_pxl_4>(~0);
			write<V_sync_line_aft_pxl_5>(~0);
			write<V_sync_line_aft_pxl_6>(~0);
			write<Vact_space_1>(~0);
			write<Vact_space_2>(~0);
			write<Vact_space_3>(~0);
			write<Vact_space_4>(~0);
			write<Vact_space_5>(~0);
			write<Vact_space_6>(~0);

			/* timing generator config */
			write<H_fsz>(2200);
			write<Hact_st>(280);
			write<Hact_sz>(1920);
			write<V_fsz>(1125);
			write<Vsync>(1);
			write<Vsync2>(563);
			write<Vact_st>(45);
			write<Vact_sz>(1080);
			write<Field_chg>(563);
			write<Vact_st2>(584);
			write<Vact_st3>(1147);
			write<Vact_st4>(1710);
			write<Vsync_top_hdmi>(1);
			write<Vsync_bot_hdmi>(563);
			write<Field_top_hdmi>(1);
			write<Field_bot_hdmi>(563);
			write<Fp_3d::Value>(0);
		}

		I2c_hdmi _i2c_hdmi;

	public:

		/**
		 * Constructor
		 */
		Hdmi()
		: Attached_mmio(Genode::Board_base::HDMI_BASE, 0xa0000), _i2c_hdmi() { }

		/**
		 * Initialize HDMI controller for video output only
		 *
		 * \param scr_width   screen width in pixel
		 * \param scr_height  screen height in pixel
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int init_hdmi(unsigned scr_width, unsigned scr_height)
		{
			/* choose appropriate output mode and parameters */
			enum Aspect_ratio { _16_9 };
			enum { VREFRESH_HZ = 60 };
			Aspect_ratio aspect_ratio;
			unsigned     pixel_clk;
			uint8_t      cea_video_mode;

			/* FIXME: see edid_cea_modes in drm_edid.c in Linaro */
			if (scr_width == 1920 && scr_height == 1080 && VREFRESH_HZ == 60) {
				pixel_clk      = 148500000;
				aspect_ratio   = _16_9;
				cea_video_mode = 16;
			} else {
				error("resolution not supported");
				return -1;
			}
			/* set-up HDMI PHY */
			write<Phy_con_0::Pwr_off>(0);
			if (_i2c_hdmi.stop_hdmi_phy()) return -1;
			write<Phy_rstout::Reset>(1);
			delayer()->usleep(10000);
			write<Phy_rstout::Reset>(0);
			delayer()->usleep(10000);
			if (_i2c_hdmi.setup_and_start_hdmi_phy(pixel_clk)) return -1;

			/* reset HDMI CORE */
			write<Core_rstout::Reset>(0);
			delayer()->usleep(10000);
			write<Core_rstout::Reset>(1);
			delayer()->usleep(10000);

			/* common config */
			write<Intc_con_0::En_global>(0);
			write<Mode_sel::Mode>(2); /* HDMI mode */
			write<Con_0::Blue_scr_en>(0);
			write<Avi_con::Tx_con>(2); /* transmit on every VSYNC */

			/* AVI packet config: header */
			enum {
				INFOFRAME   = 0x80,
				AVI         = 0x02,
				TYPE        = INFOFRAME | AVI,
				VERSION     = 2,
				LENGTH      = 13,
				HDR_CHK_SUM = TYPE + VERSION + LENGTH,
			};
			write<Avi_header_0>(TYPE);
			write<Avi_header_1>(VERSION);
			write<Avi_header_2>(LENGTH);

			/* AVI packet config: data byte 1 */
			enum {
				UNDERSCANNED_DISPL = 1 << 1,
				ACTIVE_FORMAT      = 1 << 4,
				RGB                = 0 << 5,
				OUT_FORMAT = UNDERSCANNED_DISPL | ACTIVE_FORMAT | RGB,
			};
			write<Avi_data_1>(OUT_FORMAT);

			/* AVI packet config: data byte 2 */
			enum {
				PIC_RATIO_16_9        = 0x20,
				AVI_RATIO_SAME_AS_PIC = 0x08,
			};
			switch (aspect_ratio) {
			case _16_9:
				write<Avi_data_2>(PIC_RATIO_16_9 | AVI_RATIO_SAME_AS_PIC);
				break;
			default:
				error("aspect ratio not supported");
				return -1;
			}
			write<Avi_data_4>(cea_video_mode);

			/* AVI packet config: checksum */
			Avi_check_sum::access_t acs = HDR_CHK_SUM;
			acs += read<Avi_data_1>();
			acs += read<Avi_data_2>();
			acs += read<Avi_data_3>();
			acs += read<Avi_data_4>();
			acs += read<Avi_data_5>();
			acs += read<Avi_data_6>();
			acs += read<Avi_data_7>();
			acs += read<Avi_data_8>();
			acs += read<Avi_data_9>();
			acs += read<Avi_data_10>();
			acs += read<Avi_data_11>();
			acs += read<Avi_data_12>();
			acs += read<Avi_data_13>();
			acs = 0x100 - (acs & 0xff);
			write<Avi_check_sum>(acs);

			/**
			 * FIXME: At this point Linaro writes AUI infoframe. For more
			 *        info see hdmi_conf_init in exynos_hdmi.c.
			 */

			/**
			 * FIXME: At this point Linaro attempts to limit pixel values
			 *        wich fails due to a SW bug and seems to be not necessary
			 *        anyways (see hdmi_conf_init in exynos_hdmi.c).
			 */

			/**
			 * FIXME: At this point Linux configures audio. For more
			 *        info see hdmi_audio_init in exynos_hdmi.c.
			 */

			/* do video and timing-generator config */
			switch (cea_video_mode) {
			case 16:
				_setup_mode_16();
				break;
			default:
				error("mode not supported");
				return -1;
			}
			/* wait for PHY PLLs to get steady */
			if (!wait_for<Phy_status_0::Phy_ready>(1, *delayer(), 10)) {
				error("HDMI PHY not ready");
				return -1;
			}
			/* turn on core and timing generator */
			write<Con_0::System_en>(1);
			write<Cmd::Tg_en>(1);

			/**
			 * FIXME: At this point Linaro turns Audio on.
			 *        See hdmi_audio_control in exynos_hdmi.c.
			 */

			return 0;
		}
};


/*************************
 ** Framebuffer::Driver **
 *************************/

int Framebuffer::Driver::init_drv(size_t width, size_t height, Format format,
                                  Output output, addr_t fb_phys)
{
	_fb_width  = width;
	_fb_height = height;
	_fb_format = format;

	/* set-up targeted output */
	switch (output) {
	case OUTPUT_HDMI:
		if (_init_hdmi(fb_phys)) { return -1; }
		return 0;
	default:
		error("output not supported");
		return -1;
	}
}


int Framebuffer::Driver::_init_hdmi(addr_t fb_phys)
{
	/* feed in power and clocks */
	hdmi_clock()->state(1);
	hdmi_power()->state(1);

	/* set-up video mixer to feed HDMI */
	int err;
	err = video_mixer()->init_mxr(fb_phys, _fb_width, _fb_height, _fb_format);
	if (err) { return -1; }

	/* set-up HDMI to feed connected device */
	static Hdmi hdmi;
	err = hdmi.init_hdmi(_fb_width, _fb_height);
	if (err) { return -1; }
	return 0;
}
