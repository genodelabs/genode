/*
 * \brief  Interrupt controller for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VEA9X4__NO_TRUSTZONE__PIC_H_
#define _VEA9X4__NO_TRUSTZONE__PIC_H_

/* core includes */
#include <pic/cortex_a9_no_trustzone.h>

namespace Kernel
{
	/**
	 * Interrupt controller for kernel
	 */
	class Pic : public Cortex_a9_no_trustzone::Pic { };
}

#endif /* _VEA9X4__NO_TRUSTZONE__PIC_H_ */

