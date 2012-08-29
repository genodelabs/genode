#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/cap_map.h>
#include <base/stdint.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
#include <l4/sys/task.h>

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

	struct Capability
	{
		static bool valid(l4_cap_idx_t idx) {
			return !(idx & L4_INVALID_CAP_BIT) && idx != 0; }
	};

}

namespace Genode {

	typedef volatile int         Native_lock;
	typedef Fiasco::l4_cap_idx_t Native_thread_id;
	typedef Fiasco::l4_cap_idx_t Native_thread;
	typedef Fiasco::l4_cap_idx_t Native_task;
	typedef Fiasco::l4_utcb_t*   Native_utcb;


	/**
	 * Native_capability in Fiasco.OC is just a reference to a Cap_index.
	 *
	 * As Cap_index objects cannot be copied around, but Native_capability
	 * have to, we have to use this indirection. Moreover, it might instead
	 * of a Cap_index reference some process-local object, and thereby
	 * implements a local capability.
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
			void*      _ptr;

		protected:

			/**
			 * Constructs a local capability, used by derived Capability
			 * class only
			 *
			 * \param ptr pointer to process-local object
			 */
			Native_capability(void* ptr) : _idx(0), _ptr(ptr) { }

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
			Native_capability() : _idx(0), _ptr(0) { }

			/**
			 * Construct capability manually
			 */
			Native_capability(Cap_index* idx)
				: _idx(idx), _ptr(0) { _inc(); }

			Native_capability(const Native_capability &o)
			: _idx(o._idx), _ptr(o._ptr) { _inc(); }

			~Native_capability()
			{
				_dec();
			}

			/**
			 * Return Cap_index object referenced by this object
			 */
			Cap_index* idx() const { return _idx; }

			/**
			 * Overloaded comparision operator
			 */
			bool operator==(const Native_capability &o) const {
				return (_ptr) ? _ptr == o._ptr : _idx == o._idx; }

			Native_capability& operator=(const Native_capability &o){
				if (this == &o)
					return *this;

				_dec();
				_ptr = o._ptr;
				_idx = o._idx;
				_inc();
				return *this;
			}

			/*******************************************
			 **  Interface provided by all platforms  **
			 *******************************************/

			int   local_name() const { return _idx ? _idx->id() : 0;        }
			Dst   dst()        const { return _idx ? Dst(_idx->kcap()) : Dst(); }
			bool  valid()      const { return (_idx != 0) && _idx->valid(); }
			void *local()      const { return _ptr;                         }
	};


	typedef int Native_connection_state;

	struct Native_config
	{
		/**
		 * Thread-context area configuration.
		 */
		static addr_t context_area_virtual_base() { return 0x40000000UL; }
		static addr_t context_area_virtual_size() { return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static addr_t context_virtual_size() { return 0x00100000UL; }
	};
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
