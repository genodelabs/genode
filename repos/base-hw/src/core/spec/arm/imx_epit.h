/*
 * \brief  Timer driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2017 Kernel Labs GmbH
 *
 * This file is part of the Kernel OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__ARM__IMX_EPIT_H_
#define _SRC__CORE__SPEC__ARM__IMX_EPIT_H_

/* Kernel includes */
#include <util/mmio.h>

namespace Board { class Timer; }


/**
 * Timer driver for core
 */
struct Board::Timer : Genode::Mmio<0x14>
{
	enum { TICS_PER_MS = 33333 };

	/**
	 * Control register
	 */
	struct Cr : Register<0x0, 32>
	{
		struct En : Bitfield<0, 1> { };       /* enable timer */

		struct En_mod : Bitfield<1, 1>        /* reload on enable */
		{
			enum { RELOAD = 1 };
		};

		struct Oci_en : Bitfield<2, 1> { };   /* interrupt on compare */

		struct Prescaler : Bitfield<4, 12>    /* clock input divisor */
		{
			enum { DIVIDE_BY_1 = 0 };
		};

		struct Swr     : Bitfield<16, 1> { }; /* software reset bit */
		struct Iovw    : Bitfield<17, 1> { }; /* enable overwrite */

		struct Clk_src : Bitfield<24, 2>      /* select clock input */
		{
			enum { HIGH_FREQ_REF_CLK = 2 };
		};
	};

	/**
	 * Status register
	 */
	struct Sr : Register<0x4, 32>
	{
		struct Ocif : Bitfield<0, 1> { }; /* IRQ status, write 1 clears */
	};

	struct Lr   : Register<0x8,  32> { }; /* load value register */
	struct Cmpr : Register<0xc,  32> { }; /* compare value register */
	struct Cnt  : Register<0x10, 32> { }; /* counter register */

	/**
	 * Disable timer and clear its interrupt output
	 */
	void reset()
	{
		/* wait until ongoing reset operations are finished */
		while (read<Cr::Swr>()) ;

		/* disable timer */
		write<Cr::En>(0);

		/* clear interrupt */
		write<Sr::Ocif>(1);
	}

	Timer(unsigned);

	void init();
};

#endif /* _SRC__CORE__SPEC__ARM__IMX_EPIT_H_ */
