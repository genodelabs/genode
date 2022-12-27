/*
 * \brief  VMM - board definitions for ARM 64-bit
 * \author Stefan Kalkowski
 * \date   2019-11-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__SPEC__ARM_V8__BOARD_H_
#define _SRC__SERVER__VMM__SPEC__ARM_V8__BOARD_H_

#include <board_base.h>

namespace Vmm {
	enum { KERNEL_OFFSET = 0x80000, };
}

#endif /* _SRC__SERVER__VMM__SPEC__ARM_V8__BOARD_H_ */
