/*
 * \brief  Driver for Freescale's AIPSTZ bridge
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARM__IMX_AIPSTZ_H_
#define _SRC__BOOTSTRAP__SPEC__ARM__IMX_AIPSTZ_H_

#include <util/mmio.h>

namespace Bootstrap {

	/**
	 * AHB to IP Bridge
	 *
	 * Interface between the system bus and lower bandwidth IP Slave (IPS)
	 * bus peripherals.
	 */
	class Aipstz;
}


class Bootstrap::Aipstz : public Genode::Mmio<0x54>
{
	private:

		/*
		 * Configuration of the masters
		 */

		struct Mpr { enum { ALL_UNBUFFERED_AND_FULLY_TRUSTED = 0x77777777 }; };
		struct Mpr1 : Register<0x0, 32>, Mpr { };
		struct Mpr2 : Register<0x4, 32>, Mpr { };

		/*
		 * Configuration of the platform peripherals
		 */

		struct Pacr { enum { ALL_UNBUFFERED_AND_FULLY_UNPROTECTED = 0 }; };
		struct Pacr1 : Register<0x20, 32>, Pacr { };
		struct Pacr2 : Register<0x24, 32>, Pacr { };
		struct Pacr3 : Register<0x28, 32>, Pacr { };
		struct Pacr4 : Register<0x2c, 32>, Pacr { };

		/*
		 * Configuration of the off-platform peripherals
		 */

		struct Opacr1 : Register<0x40, 32>, Pacr { };
		struct Opacr2 : Register<0x44, 32>, Pacr { };
		struct Opacr3 : Register<0x48, 32>, Pacr { };
		struct Opacr4 : Register<0x4c, 32>, Pacr { };
		struct Opacr5 : Register<0x50, 32>, Pacr { };

	public:

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		Aipstz(Genode::addr_t const base) : Mmio({(char *)base, Mmio::SIZE})
		{
			/* avoid AIPS intervention at any memory access */
			write<Mpr1>(Mpr::ALL_UNBUFFERED_AND_FULLY_TRUSTED);
			write<Mpr2>(Mpr::ALL_UNBUFFERED_AND_FULLY_TRUSTED);
			write<Pacr1>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Pacr2>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Pacr3>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Pacr4>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Opacr1>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Opacr2>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Opacr3>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Opacr4>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			write<Opacr5>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
		}
};

#endif /* _SRC__BOOTSTRAP__SPEC__ARM__IMX_AIPSTZ_H_ */
