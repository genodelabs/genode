/*
 * \brief   Static kernel configuration
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__CONFIGURATION_H_
#define _KERNEL__CONFIGURATION_H_

namespace Kernel
{
	enum {
		DEFAULT_STACK_SIZE   = 16 * 1024,
		USER_LAP_TIME_MS     = 100,
		MAX_PDS              = 256,
		MAX_THREADS          = 256,
		MAX_SIGNAL_RECEIVERS = 256,
		MAX_SIGNAL_CONTEXTS  = 2048,
		MAX_VMS              = 4,
		MAX_PRIORITY         = 255,
	};
}

#endif /* _KERNEL__CONFIGURATION_H_ */
