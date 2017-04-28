/*
 * \brief  IOMUX controller register description
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__IMX53__IOMUX_H_
#define _DRIVERS__PLATFORM__SPEC__IMX53__IOMUX_H_

/* Genode includes */
#include <util/mmio.h>
#include <drivers/defs/imx53.h>
#include <base/attached_io_mem_dataspace.h>

class Iomux : public Genode::Attached_io_mem_dataspace,
               Genode::Mmio
{
	private:

		struct Gpr2 : Register<0x8,32>
		{
			struct Ch1_mode        : Bitfield<2, 2> {
					enum { ROUTED_TO_DI1 = 0x3 }; };
			struct Data_width_ch1  : Bitfield<7, 1> {
					enum { PX_18_BITS, PX_24_BITS }; };
			struct Bit_mapping_ch1 : Bitfield<8, 1> {
					enum { SPWG, JEIDA }; };
			struct Di1_vs_polarity : Bitfield<10,1> { };
		};

		struct Key_col3 : Register<0x3c, 32> {};
		struct Key_row3 : Register<0x40, 32> {};

		struct Eim_a24 : Register<0x15c, 32> { };

		template <unsigned OFF>
		struct Sw_mux_ctl_pad_gpio : Register<0x314 + OFF*4, 32> { };

		struct Sw_pad_ctl_pad_key_col3 : Register<0x364, 32> { };
		struct Sw_pad_ctl_pad_key_row3 : Register<0x368, 32> { };

		struct Sw_pad_ctl_pad_eim_a24 : Register<0x4a8, 32> { };

		template <unsigned OFF>
		struct Sw_pad_ctl_pad_gpio : Register<0x6a4 + OFF*4, 32> { };

		struct I2c2_ipp_scl_in_select_input : Register<0x81c, 32> { };
		struct I2c2_ipp_sda_in_select_input : Register<0x820, 32> { };
		struct I2c3_ipp_scl_in_select_input : Register<0x824, 32> { };
		struct I2c3_ipp_sda_in_select_input : Register<0x828, 32> { };

	public:

		Iomux(Genode::Env &env)
		: Genode::Attached_io_mem_dataspace(env, Imx53::IOMUXC_BASE,
		                                         Imx53::IOMUXC_SIZE),
		  Genode::Mmio((Genode::addr_t)local_addr<void>())
		{ }

		void i2c_2_enable()
		{
			write<Key_col3>(0x14);
			write<I2c2_ipp_scl_in_select_input>(0);
			write<Sw_pad_ctl_pad_key_col3>(0x12d);
			write<Key_row3>(0x14);
			write<I2c2_ipp_sda_in_select_input>(0);
			write<Sw_pad_ctl_pad_key_row3>(0x12d);
		}

		void i2c_3_enable()
		{
			write<Sw_mux_ctl_pad_gpio<3> >(0x12);
			write<I2c3_ipp_scl_in_select_input>(0x1);
			write<Sw_pad_ctl_pad_gpio<3> >(0x12d);
			write<Sw_mux_ctl_pad_gpio<4> >(0x12);
			write<I2c3_ipp_sda_in_select_input>(0x1);
			write<Sw_pad_ctl_pad_gpio<4> >(0x12d);
		}

		void ipu_enable()
		{
			write<Gpr2::Di1_vs_polarity>(1);
			write<Gpr2::Data_width_ch1>(Gpr2::Data_width_ch1::PX_18_BITS);
			write<Gpr2::Bit_mapping_ch1>(Gpr2::Bit_mapping_ch1::SPWG);
			write<Gpr2::Ch1_mode>(Gpr2::Ch1_mode::ROUTED_TO_DI1);
		}

		void pwm_enable()
		{
			write<Eim_a24>(1);
			write<Sw_pad_ctl_pad_eim_a24>(0);
			write<Sw_mux_ctl_pad_gpio<1> >(0x4);
			write<Sw_pad_ctl_pad_gpio<1> >(0x0);
		}

		void buttons_enable()
		{
			write<Eim_a24>(1);
			write<Sw_pad_ctl_pad_eim_a24>(0);
		}
};

#endif /* _DRIVERS__PLATFORM__SPEC__IMX53__IOMUX_H_ */
