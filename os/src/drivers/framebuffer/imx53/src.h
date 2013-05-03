/*
 * \brief  System reset controller registers
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-11-06
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC_H_
#define _SRC_H_

/* Genode includes */
#include <util/mmio.h>

struct Src : Genode::Mmio
{
	Src(Genode::addr_t const mmio_base) : Genode::Mmio(mmio_base) { }

	struct Ctrl_reg : Register<0x0, 32>
	{
		struct Ipu_rst : Bitfield<3, 1> { };
	};
};

#endif /* _SRC_H_ */
