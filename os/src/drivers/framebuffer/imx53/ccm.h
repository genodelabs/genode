/*
 * \brief  Clock control module
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-10-09
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CCM_H_
#define _CCM_H_

/* Genode includes */
#include <util/mmio.h>

struct Ccm : Genode::Mmio
{
	enum { IPU_CLK = 133000000 };

	/**
	 * Control divider register
	 */
	struct Ccdr : Register<0x4, 32>
	{
		struct Ipu_hs_mask : Bitfield <21, 1> { };
	};

	/**
	 * Low power control register
	 */
	struct Clpcr : Register<0x54, 32>
	{
		struct Bypass_ipu_hs : Bitfield<18, 1> { };
	};

	/**
	 *
	 */
	struct Cccr5 : Register<0x7c, 32>
	{
		struct Ipu_clk_en : Bitfield<10, 2> { };
	};

	void ipu_clk_enable(void)
	{
		write<Cccr5::Ipu_clk_en>(3);
		write<Ccdr::Ipu_hs_mask>(0);
		write<Clpcr::Bypass_ipu_hs>(0);
	}

	void ipu_clk_disable(void)
	{
		write<Cccr5::Ipu_clk_en>(0);
		write<Ccdr::Ipu_hs_mask>(1);
		write<Clpcr::Bypass_ipu_hs>(1);
	}

	Ccm(Genode::addr_t const mmio_base) : Genode::Mmio(mmio_base) { }

};

#endif /* _CCM_H_ */
