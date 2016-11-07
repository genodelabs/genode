/*
 * \brief  Kernel-specific raw-output back end
 * \author Norman Feske
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__RAW_WRITE_STRING_H_
#define _INCLUDE__BASE__INTERNAL__RAW_WRITE_STRING_H_

#include <util/string.h>
#include <linux_syscalls.h>

namespace Genode {

	void raw_write_string(const char *str)
	{
		lx_syscall(SYS_write, (int)1, str, strlen(str));
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__RAW_WRITE_STRING_H_ */
