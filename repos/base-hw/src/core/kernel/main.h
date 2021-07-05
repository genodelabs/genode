/*
 * \brief  Main object of the kernel
 * \author Martin Stein
 * \date   2021-07-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KERNEL__MAIN_H_
#define _KERNEL__MAIN_H_

/* base-hw Core includes */
#include <kernel/types.h>

namespace Kernel {

	void main_handle_kernel_entry();

	void main_initialize_and_handle_kernel_entry();

	time_t main_read_idle_thread_execution_time(unsigned cpu_idx);
}

#endif /* _KERNEL__MAIN_H_ */
