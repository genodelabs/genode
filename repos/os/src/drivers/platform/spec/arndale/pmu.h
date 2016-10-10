/*
 * \brief  Regulator driver for power management unit of Exynos5250 SoC
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__ARNDALE__PMU_H_
#define _DRIVERS__PLATFORM__SPEC__ARNDALE__PMU_H_

#include <base/log.h>
#include <regulator/consts.h>
#include <regulator/driver.h>
#include <drivers/board_base.h>
#include <os/attached_mmio.h>

using namespace Regulator;
using Genode::warning;

class Pmu : public Regulator::Driver,
            public Genode::Attached_mmio
{
	private:

		template <unsigned OFFSET>
		struct Control : Register <OFFSET, 32>
		{
			struct Enable : Register<OFFSET, 32>::template Bitfield<0, 1> { };
		};

		template <unsigned OFFSET>
		struct Configuration : Register <OFFSET, 32>
		{
			struct Local_pwr_cfg : Register<OFFSET, 32>::template Bitfield<0, 3> { };
		};

		template <unsigned OFFSET>
		struct Status : Register <OFFSET, 32>
		{
			struct Stat : Register<OFFSET, 32>::template Bitfield<0, 3> { };
		};

		template <unsigned OFFSET>
		struct Sysclk_configuration : Register <OFFSET, 32>
		{
			struct Local_pwr_cfg : Register<OFFSET, 32>::template Bitfield<0, 1> { };
		};

		template <unsigned OFFSET>
		struct Sysclk_status : Register <OFFSET, 32>
		{
			struct Stat : Register<OFFSET, 32>::template Bitfield<0, 1> { };
		};

		struct Hdmi_phy_control : Register<0x700, 32>
		{
			struct Enable    : Bitfield<0, 1> { };
			struct Div_ratio : Bitfield<16, 10> { };
		};

		typedef Control<0x704> Usbdrd_phy_control;
		typedef Control<0x708> Usbhost_phy_control;
		typedef Control<0x70c> Efnand_phy_control;
		typedef Control<0x718> Adc_phy_control;
		typedef Control<0x71c> Mtcadc_phy_control;
		typedef Control<0x720> Dptx_phy_control;
		typedef Control<0x724> Sata_phy_control;

		typedef Sysclk_configuration<0x2a40> Vpll_sysclk_configuration;
		typedef Sysclk_status<0x2a44>        Vpll_sysclk_status;
		typedef Sysclk_configuration<0x2a60> Epll_sysclk_configuration;
		typedef Sysclk_status<0x2a64>        Epll_sysclk_status;
		typedef Sysclk_configuration<0x2aa0> Cpll_sysclk_configuration;
		typedef Sysclk_status<0x2aa4>        Cpll_sysclk_status;
		typedef Sysclk_configuration<0x2ac0> Gpll_sysclk_configuration;
		typedef Sysclk_status<0x2ac4>        Gpll_sysclk_status;

		typedef Configuration<0x4000> Gscl_configuration;
		typedef Status<0x4004>        Gscl_status;
		typedef Configuration<0x4020> Isp_configuration;
		typedef Status<0x4024>        Isp_status;
		typedef Configuration<0x4040> Mfc_configuration;
		typedef Status<0x4044>        Mfc_status;
		typedef Configuration<0x4060> G3d_configuration;
		typedef Status<0x4064>        G3d_status;
		typedef Configuration<0x40A0> Disp1_configuration;
		typedef Status<0x40A4>        Disp1_status;
		typedef Configuration<0x40C0> Mau_configuration;
		typedef Status<0x40C4>        Mau_status;


		template <typename C, typename S>
		void _disable_domain()
		{
			if (read<typename S::Stat>() == 0)
				return;
			write<typename C::Local_pwr_cfg>(0);
			while (read<typename S::Stat>() != 0) ;
		}

		template <typename C, typename S>
		void _enable_domain()
		{
			if (read<typename S::Stat>() == 7)
				return;
			write<typename C::Local_pwr_cfg>(7);
			while (read<typename S::Stat>() != 7) ;
		}

		void _enable(unsigned long id)
		{
			switch (id) {
			case PWR_USB30:
				write<Usbdrd_phy_control::Enable>(1);
				break;
			case PWR_USB20:
				write<Usbhost_phy_control::Enable>(1);
				break;
			case PWR_SATA :
				write<Sata_phy_control::Enable>(1);
				break;
			case PWR_HDMI: {
				_enable_domain<Disp1_configuration, Disp1_status>();
				Hdmi_phy_control::access_t hpc = read<Hdmi_phy_control>();
				Hdmi_phy_control::Div_ratio::set(hpc, 150);
				Hdmi_phy_control::Enable::set(hpc, 1);
				write<Hdmi_phy_control>(hpc);
				break; }
			default:
				warning("Unsupported for ", names[id].name);
			}
		}

		void _disable(unsigned long id)
		{
			switch (id) {
			case PWR_USB30:
				write<Usbdrd_phy_control::Enable>(0);
				break;
			case PWR_USB20:
				write<Usbhost_phy_control::Enable>(0);
				break;
			case PWR_SATA :
				write<Sata_phy_control::Enable>(0);
				break;
			default:
				warning("Unsupported for ", names[id].name);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Pmu() : Genode::Attached_mmio(Genode::Board_base::PMU_MMIO_BASE,
		                              Genode::Board_base::PMU_MMIO_SIZE)
		{
			write<Hdmi_phy_control   ::Enable>(0);
			write<Usbdrd_phy_control ::Enable>(0);
			write<Usbhost_phy_control::Enable>(0);
			write<Efnand_phy_control ::Enable>(0);
			write<Adc_phy_control    ::Enable>(0);
			write<Mtcadc_phy_control ::Enable>(0);
			write<Dptx_phy_control   ::Enable>(0);
			write<Sata_phy_control   ::Enable>(0);

			_disable_domain<Gscl_configuration,  Gscl_status>();
			_disable_domain<Isp_configuration,   Isp_status>();
			_disable_domain<Mfc_configuration,   Mfc_status>();
			_disable_domain<G3d_configuration,   G3d_status>();
			_disable_domain<Disp1_configuration, Disp1_status>();
			_disable_domain<Mau_configuration,   Mau_status>();

			_disable_domain<Vpll_sysclk_configuration, Vpll_sysclk_status>();
			_disable_domain<Epll_sysclk_configuration, Epll_sysclk_status>();
			_disable_domain<Cpll_sysclk_configuration, Cpll_sysclk_status>();
			_disable_domain<Gpll_sysclk_configuration, Gpll_sysclk_status>();
		}


		/********************************
		 ** Regulator driver interface **
		 ********************************/

		void level(Regulator_id id, unsigned long level)
		{
			switch (id) {
			default:
				warning("Unsupported for ", names[id].name);
			}
		}

		unsigned long level(Regulator_id id)
		{
			switch (id) {
			default:
				warning("Unsupported for ", names[id].name);
			}
			return 0;
		}

		void state(Regulator_id id, bool enable)
		{
			if (enable)
				_enable(id);
			else
				_disable(id);
		}

		bool state(Regulator_id id)
		{
			switch (id) {
			case PWR_USB30:
				return read<Usbdrd_phy_control::Enable>();
			case PWR_USB20:
				return read<Usbhost_phy_control::Enable>();
			case PWR_SATA:
				return read<Sata_phy_control::Enable>();
			default:
				warning("Unsupported for ", names[id].name);
			}
			return true;
		}
};

#endif /* _DRIVERS__PLATFORM__SPEC__ARNDALE__PMU_H_ */
