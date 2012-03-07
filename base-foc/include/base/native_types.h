#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <util/string.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>

	class Fiasco_capability
	{
		private:

			l4_cap_idx_t _cap_idx;

		public:

			enum Cap_selectors {
				INVALID_CAP      = L4_INVALID_CAP,
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

			Fiasco_capability(l4_cap_idx_t cap = L4_INVALID_CAP)
			: _cap_idx(cap) { }

			Fiasco_capability(void* cap)
			: _cap_idx((l4_cap_idx_t)cap) { }

			bool valid() const { return !(_cap_idx & Fiasco::L4_INVALID_CAP_BIT)
				                        && _cap_idx != 0; }

			operator l4_cap_idx_t () { return _cap_idx; }
	};

	enum Utcb_regs {
		UTCB_TCR_BADGE      = 1,
		UTCB_TCR_THREAD_OBJ = 2
	};
}

namespace Genode {

	typedef volatile int              Native_lock;
	typedef Fiasco::Fiasco_capability Native_thread_id;
	typedef Fiasco::Fiasco_capability Native_thread;
	typedef Fiasco::Fiasco_capability Native_task;
	typedef Fiasco::l4_utcb_t*        Native_utcb;

	class Native_capability
	{
		private:

			Native_thread _cap_sel;
			int           _unique_id;

		protected:

			Native_capability(void* ptr) : _unique_id((int)ptr) { }

		public:

			/**
			 * Default constructor creates an invalid capability
			 */
			Native_capability() : _unique_id(0)  { }

			/**
			 * Construct capability manually
			 */
			Native_capability(Native_thread cap_sel, int unique_id)
			: _cap_sel(cap_sel), _unique_id(unique_id) { }

			int              local_name() const { return _unique_id;         }
			void*            local()      const { return (void*)_unique_id;  }
			Native_thread    dst()        const { return _cap_sel;           }
			Native_thread_id tid()        const { return _cap_sel;           }

			bool valid() const { return _cap_sel.valid() && _unique_id != 0; }

			/**
			 * Copy this capability to another pd.
			 */
			void copy_to(void* dst) {
				memcpy(dst, this, sizeof(Native_capability)); }
	};

	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
