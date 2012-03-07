/*
 * \brief  Native types on Pistachio
 * \author Norman Feske
 * \date   2008-07-26
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <util/string.h>

namespace Pistachio {
#include <l4/types.h>
}

namespace Genode {

	typedef volatile int Native_lock;

	class Platform_thread;

	typedef Pistachio::L4_ThreadId_t Native_thread_id;

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
		 * Pistachio has no server-defined badges for page-fault messages.
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
	 * On Pistachio, the local_name member of a capability is global to the
	 * whole system. Therefore, capabilities are to be created at a central
	 * place that prevents id clashes.
	 */
	class Native_capability
	{
		protected:

			Pistachio::L4_ThreadId_t _tid;
			long                     _local_name;

		protected:

			Native_capability(void* ptr) : _local_name((long)ptr) {}

		public:

			/**
			 * Default constructor
			 */
			Native_capability() : _local_name (0)
			{
				using namespace Pistachio;
				_tid = L4_nilthread;
			}

			long local_name() const { return _local_name; }
			Pistachio::L4_ThreadId_t dst() const { return _tid; }

			void* local() const { return (void*)_local_name; }

			bool valid() const { return !Pistachio::L4_IsNilThread(_tid); }


			/********************************************************
			 ** Functions to be used by the Pistachio backend only **
			 ********************************************************/

			/**
			 * Constructor
			 *
			 * Creates a L4 capability manually. This must not be called from
			 * generic code.
			 */
			Native_capability(Pistachio::L4_ThreadId_t tid, long local_name)
			: _tid(tid), _local_name(local_name) { }

			/**
			 * Access raw capability data
			 */
			Pistachio::L4_ThreadId_t tid() const { return _tid; };

			/**
			 * Copy this capability to another pd.
			 */
			void copy_to(void* dst) {
				memcpy(dst, this, sizeof(Native_capability)); }
	};

	typedef Pistachio::L4_ThreadId_t Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
