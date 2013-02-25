/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Martin Stein
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__PLATFORM__MAIN_HELPER_H_
#define _SRC__PLATFORM__MAIN_HELPER_H_

#include <base/native_types.h>


Genode::Native_thread_id main_thread_tid;


static void main_thread_bootstrap()
{
	main_thread_tid = Kernel::current_thread_id();
}

#endif /* _SRC__PLATFORM__MAIN_HELPER_H_ */

