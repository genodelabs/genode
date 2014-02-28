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

#ifndef _IMX31__CPU_H_
#define _IMX31__CPU_H_

/* core includes */
#include <processor_driver/arm_v6.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu : public Arm_v6::Cpu { };
}

#endif /* _IMX31__CPU_H_ */

