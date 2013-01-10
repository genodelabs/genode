/*
 * \brief  CPU driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PBXA9__CPU_H_
#define _PBXA9__CPU_H_

/* core includes */
#include <cpu/cortex_a9.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu : public Cortex_a9::Cpu { };
}

#endif /* _PBXA9__CPU_H_ */

