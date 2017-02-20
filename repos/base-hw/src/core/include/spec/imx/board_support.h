/*
 * \brief  Board driver
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

#ifndef _CORE__INCLUDE__SPEC__IMX__BOARD_SUPPORT_H_
#define _CORE__INCLUDE__SPEC__IMX__BOARD_SUPPORT_H_

/* Genode includes */
#include <drivers/board_base.h>
#include <util/mmio.h>

namespace Imx
{
	/**
	 * AHB to IP Bridge
	 *
	 * Interface between the system bus and lower bandwidth IP Slave (IPS)
	 * bus peripherals.
	 */
	class Aipstz;

	/**
	 * Board driver
	 */
	class Board;
}

class Imx::Aipstz : public Genode::Mmio
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

		Aipstz(Genode::addr_t const base) : Genode::Mmio(base) { }

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		void init()
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

class Imx::Board : public Genode::Board_base
{
	public:

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		void init()
		{
			Aipstz _aipstz_1(AIPS_1_MMIO_BASE);
			Aipstz _aipstz_2(AIPS_2_MMIO_BASE);
			_aipstz_1.init();
			_aipstz_2.init();
		}
};

#endif /* _CORE__INCLUDE__SPEC__IMX__BOARD_SUPPORT_H_ */
