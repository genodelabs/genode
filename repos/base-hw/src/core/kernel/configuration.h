/*
 * \brief   Static kernel configuration
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CONFIGURATION_H_
#define _CORE__KERNEL__CONFIGURATION_H_

#include <kernel/interface.h>

namespace Kernel {

	enum {
		DEFAULT_STACK_SIZE = 16 * 1024,
		DEFAULT_TRANSLATION_TABLE_MAX = 1024,
	};
}

#endif /* _CORE__KERNEL__CONFIGURATION_H_ */
