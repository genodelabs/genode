/*
 * \brief  Driver for Exynos-5250 soc
 * \author Martin Stein
 * \author Sebastian Sumpf
 * \date   2015-05-04
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <drivers/board_base.h>
#include <irq_session/connection.h>
#include <regulator/consts.h>
#include <regulator_session/connection.h>

#include <ahci.h>

using namespace Genode;

/**
 * I2C master interface
 */
struct I2c_interface : Attached_mmio
{
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

	Mmio::Delayer            &delayer;
	/* single-word message that starts a multi-word message transfer */
	Start_msg::access_t const start_msg;

	/**
	 * Constructor
	 *
	 * \param base        physical MMIO base
	 * \param slave_addr  ID of the targeted slave
	 */
	I2c_interface(addr_t base, unsigned slave_addr, Mmio::Delayer &delayer)
	: Attached_mmio(base, 0x10000), delayer(delayer),
	  start_msg(Start_msg::Addr::bits(slave_addr))
	{ }

	/**
	 * Wether acknowledgment for last transaction can be received
	 */
	bool ack_received()
	{
		for (unsigned i = 0; i < 3; i++) {
			if (read<Con::Irq_pending>() && !read<Stat::Last_bit>()) return 1;
			delayer.usleep(TX_DELAY_US);
		}
		Genode::error("I2C ack not received");
		return 0;
	}

	/**
	 * Wether arbitration errors occured during the last transaction
	 */
	bool arbitration_error()
	{
		if (read<Stat::Arbitr>()) {
			Genode::error("I2C arbitration failed");
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
		if (!wait_for<Stat::Busy>(0, delayer)) {
			Genode::error("I2C busy");
			return -1;
		}
		Stat::access_t stat = read<Stat>();
		Stat::Txrx_en::set(stat, 1);
		Stat::Mode::set(stat, 3);
		write<Stat>(stat);
		write<Ds>(start_msg);
		delayer.usleep(1000);
		write<Con::Tx_prescaler>(11);
		write<Stat::Busy>(1);

		/* transmit message payload */
		for (unsigned i = 0; i < msg_size; i++) {
			if (!ack_received()) return -1;
			write<Ds>(msg[i]);
			delayer.usleep(TX_DELAY_US);
			write<Con::Irq_pending>(0);
			if (arbitration_error()) return -1;
		}
		/* end message transfer */
		if (!ack_received()) return -1;
		write<Stat::Busy>(0);
		write<Con::Irq_en>(0);
		write<Con::Irq_pending>(0); /* FIXME fixup */
		if (arbitration_error()) return -1;
		if (!wait_for<Stat::Busy>(0, delayer)) {
			Genode::error("I2C end transfer failed");
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
	I2c_sataphy(Mmio::Delayer &delayer)
	: I2c_interface(0x121d0000, SLAVE_ADDR, delayer)
	{ }

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
		if (send(msg, MSG_SIZE)) return -1;
		if (verbose) Genode::log("SATA PHY 40-pin interface enabled");
		return 0;
	}

	/**
	 * Get I2C interface ready for transmissions
	 */
	void init()
	{
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
	}
};


/**
 * Classical control interface of SATA PHY-layer controller
 */
struct Sata_phy_ctrl : Attached_mmio
{
	Mmio::Delayer &delayer;
	I2c_sataphy    i2c_sataphy { delayer };

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
	Sata_phy_ctrl(Mmio::Delayer &delayer)
	: Attached_mmio(0x12170000, 0x10000), delayer(delayer)
	{
		i2c_sataphy.init();
	}

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
		if (i2c_sataphy.enable_40_pins()) return -1;

		/* Release reset */
		write<Reset::Global>(0);
		write<Reset::Global>(1);

		/*
		 * FIXME Linux reads this bit once only and continues
		 *       directly, also with zero. So if we get an error
		 *       at this point we should study the Linux behavior
		 *       in more depth.
		 */
		if (!wait_for<Phstatm::Pll_locked>(1, delayer)) {
			Genode::error("PLL lock failed");
			return -1;
		}
		if (verbose)
			Genode::log("SATA PHY initialized");

		return 0;
	}
};


struct Exynos5_hba : Platform::Hba
{
	Genode::Env &env;

	Irq_connection        irq { Board_base::SATA_IRQ };
	Regulator::Connection clock_src { env, Regulator::CLK_SATA };
	Regulator::Connection power_src { env, Regulator::PWR_SATA };

	Exynos5_hba(Genode::Env &env, Mmio::Delayer &delayer)
	: env(env)
	{
		clock_src.state(true);
		power_src.state(true);

		Sata_phy_ctrl phy(delayer);

		if (phy.init())
			throw Root::Unavailable();

		/* additionally perform some generic initializations */
		::Hba hba(*this);

		::Hba::Cap::access_t cap = hba.read< ::Hba::Cap>();
		::Hba::Cap2::access_t cap2 = hba.read< ::Hba::Cap2>();

		/* reset */
		hba.write< ::Hba::Ghc::Hr>(1);
		if (!hba.wait_for< ::Hba::Ghc::Hr>(0, ::Hba::delayer(), 1000, 1000)) {
			Genode::error("HBA reset failed");
			throw Root::Unavailable();
		}

		hba.write< ::Hba::Cap>(cap);
		hba.write< ::Hba::Cap2>(cap2);

		/* for exynos set port 0 as implemented, usally set by BIOS */
		hba.write< ::Hba::Pi>(0x1);
	}


	/*******************
	 ** Hba interface **
	 *******************/

	Genode::addr_t base() const override { return 0x122f0000; }
	Genode::size_t size() const override { return 0x10000; }

	void sigh_irq(Signal_context_capability sigh) override
	{
		irq.sigh(sigh);
		ack_irq();
	}

	void ack_irq() override { irq.ack_irq(); }

	Ram_dataspace_capability
	alloc_dma_buffer(size_t size) override
	{
		return env.ram().alloc(size, UNCACHED);
	}

	void free_dma_buffer(Ram_dataspace_capability ds)
	{
		env.ram().free(ds);
	}
};


Platform::Hba &Platform::init(Genode::Env &env, Mmio::Delayer &delayer)
{
	static Exynos5_hba h(env, delayer);
	return h;
}
