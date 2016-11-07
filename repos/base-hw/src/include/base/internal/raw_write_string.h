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

#include <kernel/interface.h>

namespace Genode {

	void raw_write_string(char const *str)
	{
		while (char c = *str++)
			Kernel::print_char(c);
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__RAW_WRITE_STRING_H_ */
