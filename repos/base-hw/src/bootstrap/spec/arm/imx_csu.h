/*
 * \brief  Driver for the Central Security Unit
 * \author Stefan Kalkowski
 * \date   2012-11-06
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARM__IMX_CSU_H_
#define _SRC__BOOTSTRAP__SPEC__ARM__IMX_CSU_H_

#include <util/mmio.h>
#include <util/register.h>

namespace Bootstrap { struct Csu; }


struct Bootstrap::Csu : Genode::Mmio<0x36c>
{
	template <Genode::off_t OFF>
	struct Csl : public Register<OFF, 32>
	{
		enum {
			SECURE   = 0x33,
			UNSECURE = 0xff,
		};

		struct Slave_a : Register<OFF, 32>::template Bitfield<0, 9>  { };
		struct Slave_b : Register<OFF, 32>::template Bitfield<16, 9> { };
	};


	struct Master : public Register<0x218, 32>
	{
		enum {
			SECURE_UNLOCKED,
			UNSECURE_UNLOCKED,
			SECURE_LOCKED,
			UNSECURE_LOCKED
		};

		struct Esdhc3 : Bitfield<0,2>  { };
		struct Cortex : Bitfield<2,2>  { };
		struct Sdma   : Bitfield<4,2>  { };
		struct Gpu    : Bitfield<6,2>  { };
		struct Usb    : Bitfield<8,2>  { };
		struct Pata   : Bitfield<10,2> { };
		struct Mlb    : Bitfield<14,2> { };
		struct Rtic   : Bitfield<18,2> { };
		struct Esdhc4 : Bitfield<20,2> { };
		struct Fec    : Bitfield<22,2> { };
		struct Dap    : Bitfield<24,2> { };
		struct Esdhc1 : Bitfield<26,2> { };
		struct Esdhc2 : Bitfield<28,2> { };
	};

	struct Alarm_mask : public Register<0x230, 32> { };
	struct Irq_ctrl   : public Register<0x368, 32> { };

	typedef Csl<0x00> Csl00;
	typedef Csl<0x04> Csl01;
	typedef Csl<0x08> Csl02;
	typedef Csl<0x0c> Csl03;
	typedef Csl<0x10> Csl04;
	typedef Csl<0x14> Csl05;
	typedef Csl<0x18> Csl06;
	typedef Csl<0x1c> Csl07;
	typedef Csl<0x20> Csl08;
	typedef Csl<0x24> Csl09;
	typedef Csl<0x28> Csl10;
	typedef Csl<0x2c> Csl11;
	typedef Csl<0x30> Csl12;
	typedef Csl<0x34> Csl13;
	typedef Csl<0x38> Csl14;
	typedef Csl<0x3c> Csl15;
	typedef Csl<0x40> Csl16;
	typedef Csl<0x44> Csl17;
	typedef Csl<0x48> Csl18;
	typedef Csl<0x4c> Csl19;
	typedef Csl<0x50> Csl20;
	typedef Csl<0x54> Csl21;
	typedef Csl<0x58> Csl22;
	typedef Csl<0x5c> Csl23;
	typedef Csl<0x60> Csl24;
	typedef Csl<0x64> Csl25;
	typedef Csl<0x68> Csl26;
	typedef Csl<0x6c> Csl27;
	typedef Csl<0x70> Csl28;
	typedef Csl<0x74> Csl29;
	typedef Csl<0x78> Csl30;
	typedef Csl<0x7c> Csl31;

	Csu(Genode::addr_t base,
	    bool secure_uart,
	    bool secure_gpio,
	    bool secure_esdhc,
	    bool secure_i2c) : Mmio({(char *)base, Mmio::SIZE})
	{
		/* Power (CCM, SRC, DPLLIP1-4, GPC and OWIRE) */
		write<Csl09::Slave_a>(Csl00::UNSECURE);

		/* AHBMAX S0-S2 */
		write<Csl09::Slave_b>(Csl00::UNSECURE);
		write<Csl20::Slave_a>(Csl00::UNSECURE);
		write<Csl06::Slave_b>(Csl00::UNSECURE);

		/* AHBMAX M6 */
		write<Csl10::Slave_a>(Csl00::UNSECURE);

		/* Timer (EPIT, GPT) TODO */
		write<Csl04::Slave_a>(Csl00::UNSECURE);

		/* UART 1-5 */
		Csl00::access_t uart_csl =
			secure_uart ? Csl00::SECURE : Csl00::UNSECURE;
		write<Csl07::Slave_b>(uart_csl);
		write<Csl08::Slave_a>(uart_csl);
		write<Csl26::Slave_a>(uart_csl);
		write<Csl30::Slave_b>(uart_csl);
		write<Csl19::Slave_a>(uart_csl);

		/* GPIO */
		Csl00::access_t gpio_csl =
			secure_gpio ? Csl00::SECURE : Csl00::UNSECURE;
		write<Csl00::Slave_b>(gpio_csl);
		write<Csl01::Slave_a>(gpio_csl);
		write<Csl01::Slave_b>(gpio_csl);
		write<Csl02::Slave_a>(gpio_csl);

		/* IOMUXC TODO */
		write<Csl05::Slave_a>(Csl00::UNSECURE);

		/* SDMA TODO */
		write<Csl15::Slave_a>(Csl00::UNSECURE);

		/* USB */
		write<Csl00::Slave_a>(Csl00::UNSECURE);

		/* TVE */
		write<Csl22::Slave_b>(Csl00::SECURE);

		/* I2C */
		Csl00::access_t i2c_csl =
			secure_i2c ? Csl00::SECURE : Csl00::UNSECURE;
		write<Csl18::Slave_a>(i2c_csl);
		write<Csl17::Slave_b>(i2c_csl);
		write<Csl31::Slave_a>(i2c_csl);

		/* IPU */
		write<Csl24::Slave_a>(Csl00::SECURE);

		/* Audio */
		write<Csl18::Slave_b>(Csl00::UNSECURE);

		/* SATA */
		write<Csl07::Slave_a>(Csl00::UNSECURE);

		/* FEC */
		write<Csl22::Slave_a>(Csl00::UNSECURE);

		/* SDHCI 1-4 */
		Csl00::access_t esdhc_csl =
			secure_esdhc ? Csl00::SECURE : Csl00::UNSECURE;
		write<Csl25::Slave_a>(esdhc_csl);
		write<Csl25::Slave_b>(esdhc_csl);
		write<Csl28::Slave_a>(esdhc_csl);
		write<Csl28::Slave_b>(esdhc_csl);

		/* SPDIF */
		write<Csl29::Slave_a>(Csl00::UNSECURE);

		/* GPU 2D */
		write<Csl24::Slave_b>(Csl00::SECURE);

		/* GPU 3D */
		write<Csl27::Slave_b>(Csl00::SECURE);

		write<Csl02::Slave_b>(Csl00::UNSECURE);
		write<Csl03::Slave_a>(Csl00::UNSECURE);
		write<Csl03::Slave_b>(Csl00::UNSECURE);
		write<Csl04::Slave_b>(Csl00::UNSECURE); // SRTC
		write<Csl05::Slave_b>(Csl00::UNSECURE);
		write<Csl06::Slave_a>(Csl00::UNSECURE);
		write<Csl08::Slave_b>(Csl00::UNSECURE);
		write<Csl10::Slave_b>(Csl00::UNSECURE);
		write<Csl11::Slave_a>(Csl00::UNSECURE);
		write<Csl11::Slave_b>(Csl00::UNSECURE);
		write<Csl12::Slave_a>(Csl00::UNSECURE);
		write<Csl12::Slave_b>(Csl00::UNSECURE);
		write<Csl13::Slave_a>(Csl00::UNSECURE);
		write<Csl13::Slave_b>(Csl00::UNSECURE);
		write<Csl14::Slave_a>(Csl00::UNSECURE);
		write<Csl14::Slave_b>(Csl00::UNSECURE);
		write<Csl15::Slave_b>(Csl00::UNSECURE); // SCC
		write<Csl16::Slave_a>(Csl00::UNSECURE);
		write<Csl16::Slave_b>(Csl00::UNSECURE); // RTIC
		write<Csl17::Slave_a>(Csl00::UNSECURE);
		write<Csl19::Slave_b>(Csl00::UNSECURE);
		write<Csl20::Slave_b>(Csl00::UNSECURE);
		write<Csl21::Slave_a>(Csl00::UNSECURE);
		write<Csl21::Slave_b>(Csl00::UNSECURE);
		write<Csl23::Slave_a>(Csl00::SECURE); //VPU
		write<Csl23::Slave_b>(Csl00::UNSECURE);
		write<Csl26::Slave_b>(Csl00::UNSECURE);
		write<Csl27::Slave_a>(Csl00::UNSECURE);
		write<Csl29::Slave_b>(Csl00::UNSECURE);
		write<Csl30::Slave_a>(Csl00::UNSECURE);
		write<Csl31::Slave_b>(Csl00::UNSECURE);

		/* DMA from graphical subsystem is considered to be secure */
		write<Master::Gpu>(Master::SECURE_UNLOCKED);

		/* all other DMA operations are insecure */
		write<Master::Sdma>(Master::UNSECURE_UNLOCKED);
		write<Master::Usb>(Master::UNSECURE_UNLOCKED);
		write<Master::Pata>(Master::UNSECURE_UNLOCKED);
		write<Master::Fec>(Master::UNSECURE_UNLOCKED);
		write<Master::Dap>(Master::UNSECURE_UNLOCKED);

		Master::access_t esdhc_master =
			secure_esdhc ? Master::SECURE_UNLOCKED
			             : Master::UNSECURE_UNLOCKED;
		write<Master::Esdhc1>(esdhc_master);
		write<Master::Esdhc2>(esdhc_master);
		write<Master::Esdhc3>(esdhc_master);
		write<Master::Esdhc4>(esdhc_master);
	}
};

#endif /* _SRC__BOOTSTRAP__SPEC__ARM__IMX_CSU_H_ */
