/*
 * \brief  System reset controller register description
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__IMX53__SRC_H_
#define _DRIVERS__PLATFORM__SPEC__IMX53__SRC_H_

/* Genode includes */
#include <util/mmio.h>
#include <drivers/board_base.h>
#include <base/attached_io_mem_dataspace.h>

class Src : public Genode::Attached_io_mem_dataspace,
            Genode::Mmio
{
	private:

		struct Ctrl_reg : Register<0x0, 32>
		{
				struct Ipu_rst : Bitfield<3, 1> { };
		};

	public:

		Src(Genode::Env &env)
		: Genode::Attached_io_mem_dataspace(env, Genode::Board_base::SRC_BASE,
		                                         Genode::Board_base::SRC_SIZE),
		  Genode::Mmio((Genode::addr_t)local_addr<void>()) {}

		void reset_ipu() { write<Ctrl_reg::Ipu_rst>(1); }
};

#endif /* _DRIVERS__PLATFORM__SPEC__IMX53__SRC_H_ */
