#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_config.h>
#include <base/cap_map.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
#include <l4/sys/task.h>

	enum Cap_selectors {

		/**********************************************
		 ** Capability seclectors controlled by core **
		 **********************************************/

		TASK_CAP         = L4_BASE_TASK_CAP, /* use the same task cap selector
		                                        like L4Re for compatibility in
		                                        L4Linux */

		/*
		 * To not clash with other L4Re cap selector constants (e.g.: L4Linux)
		 * leave the following selectors (2-7) empty
		 */

		PARENT_CAP       = 0x8UL   << L4_CAP_SHIFT, /* cap to parent session */

		/*
		 * Each thread has a designated slot in the core controlled cap
		 * selector area, where its ipc gate capability (for server threads),
		 * its irq capability (for locks), and the capability to its pager
		 * gate are stored
		 */
		THREAD_AREA_BASE = 0x9UL   << L4_CAP_SHIFT, /* offset to thread area */
		THREAD_AREA_SLOT = 0x3UL   << L4_CAP_SHIFT, /* size of one thread slot */
		THREAD_GATE_CAP  = 0,                       /* offset to the ipc gate
		                                               cap selector in the slot */
		THREAD_PAGER_CAP = 0x1UL   << L4_CAP_SHIFT, /* offset to the pager
		                                               cap selector in the slot */
		THREAD_IRQ_CAP   = 0x2UL   << L4_CAP_SHIFT, /* offset to the irq cap
		                                               selector in the slot */
		MAIN_THREAD_CAP  = THREAD_AREA_BASE + THREAD_GATE_CAP, /* shortcut to the
		                                                          main thread's
		                                                          gate cap */


		/*********************************************************
		 ** Capability seclectors controlled by the task itself **
		 *********************************************************/

		USER_BASE_CAP    = 0x200UL << L4_CAP_SHIFT,
	};

	enum Utcb_regs {
		UTCB_TCR_BADGE      = 1,
		UTCB_TCR_THREAD_OBJ = 2
	};

	struct Capability
	{
		static bool valid(l4_cap_idx_t idx) {
			return !(idx & L4_INVALID_CAP_BIT) && idx != 0; }
	};

}

namespace Genode {

	typedef Fiasco::l4_cap_idx_t Native_thread_id;
	typedef Fiasco::l4_cap_idx_t Native_thread;
	typedef Fiasco::l4_cap_idx_t Native_task;
	typedef Fiasco::l4_utcb_t*   Native_utcb;


	/**
	 * Native_capability in Fiasco.OC is just a reference to a Cap_index.
	 *
	 * As Cap_index objects cannot be copied around, but Native_capability
	 * have to, we have to use this indirection.
	 */
	class Native_capability
	{
		public:

			typedef Fiasco::l4_cap_idx_t Dst;

			struct Raw
			{
				Dst  dst;
				long local_name;
			};

		private:

			Cap_index* _idx;

		protected:

			inline void _inc()
			{
				if (_idx)
					_idx->inc();
			}

			inline void _dec()
			{
				if (_idx && !_idx->dec()) {
					cap_map()->remove(_idx);
				}
			}

		public:

			/**
			 * Default constructor creates an invalid capability
			 */
			Native_capability() : _idx(0) { }

			/**
			 * Construct capability manually
			 */
			Native_capability(Cap_index* idx)
				: _idx(idx) { _inc(); }

			Native_capability(const Native_capability &o)
			: _idx(o._idx) { _inc(); }

			~Native_capability() { _dec(); }

			/**
			 * Return Cap_index object referenced by this object
			 */
			Cap_index* idx() const { return _idx; }

			/**
			 * Overloaded comparision operator
			 */
			bool operator==(const Native_capability &o) const {
				return _idx == o._idx; }

			Native_capability& operator=(const Native_capability &o){
				if (this == &o)
					return *this;

				_dec();
				_idx = o._idx;
				_inc();
				return *this;
			}

			/*******************************************
			 **  Interface provided by all platforms  **
			 *******************************************/

			long  local_name() const { return _idx ? _idx->id() : 0;        }
			Dst   dst()        const { return _idx ? Dst(_idx->kcap()) : Dst(); }
			bool  valid()      const { return (_idx != 0) && _idx->valid(); }
	};


	typedef int Native_connection_state;

	struct Native_pd_args { };
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
