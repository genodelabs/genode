#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_capability.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>

	enum Cap_selectors {
		TASK_CAP         = L4_BASE_TASK_CAP,
		PARENT_CAP       = 0x8UL   << L4_CAP_SHIFT,
		THREADS_BASE_CAP = 0x9UL   << L4_CAP_SHIFT,
		USER_BASE_CAP    = 0x200UL << L4_CAP_SHIFT,
		THREAD_GATE_CAP  = 0,
		THREAD_PAGER_CAP = 0x1UL   << L4_CAP_SHIFT,
		THREAD_IRQ_CAP   = 0x2UL   << L4_CAP_SHIFT,
		THREAD_CAP_SLOT  = THREAD_IRQ_CAP + L4_CAP_SIZE,
		MAIN_THREAD_CAP  = THREADS_BASE_CAP + THREAD_GATE_CAP
	};

	enum Utcb_regs {
		UTCB_TCR_BADGE      = 1,
		UTCB_TCR_THREAD_OBJ = 2
	};
}

namespace Genode {

	struct Cap_dst_policy
	{
		typedef Fiasco::l4_cap_idx_t Dst;

		static bool valid(Dst idx) {
			return !(idx & Fiasco::L4_INVALID_CAP_BIT) && idx != 0; }

		static Dst invalid() { return Fiasco::L4_INVALID_CAP;}
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef volatile int         Native_lock;
	typedef Fiasco::l4_cap_idx_t Native_thread_id;
	typedef Fiasco::l4_cap_idx_t Native_thread;
	typedef Fiasco::l4_cap_idx_t Native_task;
	typedef Fiasco::l4_utcb_t*   Native_utcb;
	typedef int                  Native_connection_state;

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
