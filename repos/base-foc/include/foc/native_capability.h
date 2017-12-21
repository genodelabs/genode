/*
 * \brief  Kernel-specific capability helpers and definitions
 * \author Norman Feske
 * \date   2016-06-01
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC__NATIVE_CAPABILITY_H_
#define _INCLUDE__FOC__NATIVE_CAPABILITY_H_

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
#include <l4/sys/task.h>

	/*********************************************
	 ** Capability selectors controlled by core **
	 *********************************************/

	/* use the same task cap selector like L4Re for compatibility in L4Linux */
	static constexpr l4_cap_idx_t TASK_CAP = L4_BASE_TASK_CAP;

	static constexpr l4_cap_idx_t DEBUG_CAP = L4_BASE_DEBUGGER_CAP;

	/*
	 * To not clash with other L4Re cap selector constants (e.g.: L4Linux)
	 * leave the following selectors (2-8) empty
	 */

	/* cap to parent session */
	static constexpr l4_cap_idx_t PARENT_CAP = 0xbUL << L4_CAP_SHIFT;

	/*
	 * Each thread has a designated slot in the core controlled cap
	 * selector area, where its ipc gate capability (for server threads),
	 * its irq capability (for locks), and the capability to its pager
	 * gate are stored
	 */

	/* offset to thread area */
	static constexpr l4_cap_idx_t THREAD_AREA_BASE = 0xcUL << L4_CAP_SHIFT;

	/* size of one thread slot */
	static constexpr l4_cap_idx_t THREAD_AREA_SLOT = 0x3UL << L4_CAP_SHIFT;

	/* offset to the ipc gate cap selector in the slot */
	static constexpr l4_cap_idx_t THREAD_GATE_CAP  = 0;

	/* offset to the pager cap selector in the slot */
	static constexpr l4_cap_idx_t THREAD_PAGER_CAP = 0x1UL << L4_CAP_SHIFT;

	/* offset to the irq cap selector in the slot */
	static constexpr l4_cap_idx_t THREAD_IRQ_CAP = 0x2UL << L4_CAP_SHIFT;

	/* shortcut to the main thread's gate cap */
	static constexpr l4_cap_idx_t MAIN_THREAD_CAP = THREAD_AREA_BASE
	                                              + THREAD_GATE_CAP;


	/*********************************************************
	 ** Capability seclectors controlled by the task itself **
	 *********************************************************/

	static constexpr l4_cap_idx_t USER_BASE_CAP = 0x200UL << L4_CAP_SHIFT;

	struct Capability
	{
		static bool valid(l4_cap_idx_t idx) {
			return !(idx & L4_INVALID_CAP_BIT) && idx != 0; }
	};
}

#endif /* _INCLUDE__FOC__NATIVE_CAPABILITY_H_ */
