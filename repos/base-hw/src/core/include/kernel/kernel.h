/*
 * \brief   Singlethreaded minimalistic kernel
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-09-30
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__KERNEL_H_
#define _KERNEL__KERNEL_H_

#include <pic.h>
#include <board.h>

/**
 * Main routine of every kernel pass
 */
extern "C" void kernel();


namespace Kernel {

	class Pd;

	Pd            * core_pd();
	Pic           * pic();
	Genode::Board & board();
}

#endif /* _KERNEL__KERNEL_H_ */
