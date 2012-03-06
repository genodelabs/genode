/*
 * \brief  Platform-specific type definitions
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

namespace Genode {

	typedef volatile int  Native_lock;

	struct Native_thread
	{
		int ec_sel;      /* NOVA cap selector for execution context */
		int sc_sel;      /* NOVA cap selector for scheduling context */
		int rs_sel;      /* NOVA cap selector for running semaphore */
		int pd_sel;      /* NOVA cap selector of protection domain */
		int exc_pt_sel;  /* base of event portal window */
	};

	typedef Native_thread Native_thread_id;

	inline bool operator == (Native_thread_id t1, Native_thread_id t2) { return t1.ec_sel == t2.ec_sel; }
	inline bool operator != (Native_thread_id t1, Native_thread_id t2) { return t1.ec_sel != t2.ec_sel; }

	class Native_utcb
	{
		private:

			/**
			 * Size of the NOVA-specific user-level thread-control block
			 */
			enum { UTCB_SIZE = 4096 };

			/**
			 * User-level thread control block
			 *
			 * The UTCB is one 4K page, shared between the kernel and the
			 * user process. It is not backed by a dataspace but provided
			 * by the kernel.
			 */
			long _utcb[UTCB_SIZE/sizeof(long)];
	};

	class Native_capability
	{
		private:

			int _pt_sel;
			int _unique_id;

		protected:

			Native_capability(void* ptr) : _unique_id((int)ptr) {}

		public:

			/**
			 * Default constructor creates an invalid capability
			 */
			Native_capability() : _pt_sel(0), _unique_id(0) { }

			/**
			 * Construct capability manually
			 *
			 * This constructor should be called only from the platform-specific
			 * part of the Genode framework.
			 */
			Native_capability(int pt_sel, int unique_id)
			: _pt_sel(pt_sel), _unique_id(unique_id) { }

			bool  valid()      const { return _pt_sel != 0 && _unique_id != 0; }
			int   local_name() const { return _unique_id; }
			void* local()      const { return (void*)_unique_id; }
			int   dst()        const { return _pt_sel; }

			int   unique_id()  const { return _unique_id; }
			int   pt_sel()     const { return _pt_sel; }
	};

	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
