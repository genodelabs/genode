/*
 * \brief  Native types on L4/Fiasco
 * \author Norman Feske
 * \date   2008-07-26
 */

/*
 * Copyright (C) 2008-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

namespace Fiasco {
#include <l4/sys/types.h>

	/**
	 * Return invalid L4 thread ID
	 */
	inline l4_threadid_t invalid_l4_threadid_t() { return L4_INVALID_ID; }
}

namespace Genode {

	typedef volatile int Native_lock;

	class Platform_thread;

	typedef Fiasco::l4_threadid_t Native_thread_id;

	struct Native_thread
	{
		Native_thread_id l4id;

		/**
		 * Only used in core
		 *
		 * For 'Thread' objects created within core, 'pt' points to
		 * the physical thread object, which is going to be destroyed
		 * on destruction of the 'Thread'.
		 */
		Platform_thread *pt;
	};

	inline unsigned long convert_native_thread_id_to_badge(Native_thread_id tid)
	{
		/*
		 * Fiasco has no server-defined badges for page-fault messages.
		 * Therefore, we have to interpret the sender ID as badge.
		 */
		return tid.raw;
	}

	/**
	 * Empty UTCB type expected by the thread library
	 *
	 * On this kernel, UTCBs are not placed within the the context area. Each
	 * thread can request its own UTCB pointer using the kernel interface.
	 */
	typedef struct { } Native_utcb;

	/*
	 * On Fiasco, the local_name member of a capability is global
	 * to the whole system. Therefore, capabilities are to be
	 * created at a central place that prevents id clashes.
	 */
	class Native_capability
	{
		protected:

			Fiasco::l4_threadid_t _tid;
			long                  _local_name;

		public:

			/**
			 * Default constructor
			 */
			Native_capability()
			: _tid(Fiasco::invalid_l4_threadid_t()), _local_name(0) { }

			long local_name() const { return _local_name; }
			Fiasco::l4_threadid_t dst() const { return _tid; }

			bool valid() const { return l4_is_invalid_id(_tid) == 0; }


			/*****************************************************
			 ** Functions to be used by the Fiasco backend only **
			 *****************************************************/

			/**
			 * Constructor
			 *
			 * This constructor can be called to create a Fiasco
			 * capability by hand. It must never be used from
			 * generic code!
			 */
			Native_capability(Fiasco::l4_threadid_t tid,
			                  Fiasco::l4_umword_t local_name)
			: _tid(tid), _local_name(local_name) { }

			/**
			 * Access raw capability data
			 */
			Fiasco::l4_threadid_t tid() const { return _tid; }
	};

	typedef Fiasco::l4_threadid_t Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
