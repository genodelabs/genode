/*
 * \brief  CPU driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXYNOS5__CPU_H_
#define _EXYNOS5__CPU_H_

/* core includes */
#include <cpu/cortex_a15.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu : public Cortex_a15::Cpu { };
}

#endif /* _EXYNOS5__CPU_H_ */

