/*
 * \brief  Timer for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IMX31__TIMER_H_
#define _IMX31__TIMER_H_

/* core includes */
#include <timer/imx31.h>

namespace Kernel
{
	/**
	 * Kernel timer
	 */
	class Timer : public Imx31::Timer { };
}

#endif /* _IMX31__TIMER_H_ */

