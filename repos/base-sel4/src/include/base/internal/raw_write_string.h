/*
 * \brief  Kernel-specific raw-output back end
 * \author Norman Feske
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__RAW_WRITE_STRING_H_
#define _INCLUDE__BASE__INTERNAL__RAW_WRITE_STRING_H_

/* seL4 includes */
#include <sel4/arch/functions.h>
#include <sel4/arch/syscalls.h>

namespace Genode {

	void raw_write_string(char const *str)
	{
		while (char c = *str++)
			seL4_DebugPutChar(c);
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__RAW_WRITE_STRING_H_ */
