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

#ifndef _PMU_H_
#define _PMU_H_

#include <regulator/consts.h>
#include <regulator/driver.h>
#include <drivers/board_base.h>
#include <os/attached_mmio.h>

using namespace Regulator;


class Pmu : public Regulator::Driver,
            public Genode::Attached_mmio
{
	private:

		template <unsigned OFF>
		struct Control : Register <OFF, 32>
		{
			struct Enable : Register<OFF, 32>::template Bitfield<0, 1> { };
		};

		typedef Control<0x704> Usbdrd_phy_control;
		typedef Control<0x708> Usbhost_phy_control;
		typedef Control<0x724> Sata_phy_control;


		/***********************
		 ** USB 3.0 functions **
		 ***********************/

		void _usb30_enable()
		{
			write<Usbdrd_phy_control::Enable>(1);
			write<Usbhost_phy_control::Enable>(1);
		}

		void _usb30_disable()
		{
			write<Usbdrd_phy_control::Enable>(0);
			write<Usbhost_phy_control::Enable>(0);
		}

		bool _usb30_enabled()
		{
			return read<Usbdrd_phy_control::Enable>() &&
			       read<Usbhost_phy_control::Enable>();

		}

	public:

		/**
		 * Constructor
		 */
		Pmu() : Genode::Attached_mmio(Genode::Board_base::PMU_MMIO_BASE,
		                              Genode::Board_base::PMU_MMIO_SIZE) { }


		/********************************
		 ** Regulator driver interface **
		 ********************************/

		void set_level(Regulator_id id, unsigned long level)
		{
			switch (id) {
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
		}

		unsigned long level(Regulator_id id)
		{
			switch (id) {
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
			return 0;
		}

		void set_state(Regulator_id id, bool enable)
		{
			switch (id) {
			case PWR_USB30:
				if (enable)
					_usb30_enable();
				else
					_usb30_disable();
				break;
			case PWR_SATA :
				if (enable)
					write<Sata_phy_control::Enable>(1);
				else
					write<Sata_phy_control::Enable>(0);
				break;
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
		}

		bool state(Regulator_id id)
		{
			switch (id) {
			case PWR_USB30:
				return _usb30_enabled();
			case PWR_SATA:
				return read<Sata_phy_control::Enable>();
			default:
				PWRN("Unsupported for %s", names[id].name);
			}
			return true;
		}
};

#endif /* _PMU_H_ */
