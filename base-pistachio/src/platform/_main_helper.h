/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Christian Prochaska
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM___MAIN_HELPER_H_
#define _PLATFORM___MAIN_HELPER_H_

/* Pistachio includes */
namespace Pistachio {
#include <l4/thread.h>
}

/* Genode includes */
#include <base/native_types.h>


Genode::Native_thread_id main_thread_tid;


static void main_thread_bootstrap()
{
	main_thread_tid = Pistachio::L4_Myself();
}


#endif /* _PLATFORM___MAIN_HELPER_H_ */
