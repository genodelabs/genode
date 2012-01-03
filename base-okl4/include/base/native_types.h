/*
 * \brief  Native types on OKL4
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

namespace Okl4 { extern "C" {
#include <l4/types.h>
} }

namespace Genode {

	class Platform_thread;

	/**
	 * Index of the UTCB's thread word used for storing the own global
	 * thread ID
	 */
	enum { UTCB_TCR_THREAD_WORD_MYSELF = 0 };

	namespace Thread_id_bits {

		/*
		 * L4 thread ID has 18 bits for thread number and 14 bits for
		 * version info.
		 */
		enum { PD = 8, THREAD = 5 };
	}

	typedef Okl4::L4_ThreadId_t Native_thread_id;

	inline bool operator == (Native_thread_id t1, Native_thread_id t2) {
		return t1.raw == t2.raw; }

	inline bool operator != (Native_thread_id t1, Native_thread_id t2) {
		return t1.raw != t2.raw; }

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
		 * OKL4 has no server-defined badges for page-fault messages.
		 * Therefore, we have to interpret the sender ID as badge.
		 */
		return tid.raw;
	}

	/**
	 * Empty UTCB type expected by the thread library, unused on OKL4
	 *
	 * On this kernel, UTCBs are not placed within the the context area. Each
	 * thread can request its own UTCB pointer using the kernel interface.
	 */
	typedef struct { } Native_utcb;

	/*
	 * On OKL4, the local_name member of a capability is global to the whole
	 * system. Therefore, capabilities are to be created at a central place
	 * that prevents id clashes.
	 */
	class Native_capability
	{
		protected:

			Okl4::L4_ThreadId_t _tid;
			long                _local_name;

		public:

			/**
			 * Default constructor
			 */
			Native_capability() : _local_name(0) {
				_tid = Okl4::L4_nilthread; }

			long local_name() const { return _local_name; }
			Okl4::L4_ThreadId_t dst() const { return _tid; }

			bool valid() const { return !Okl4::L4_IsNilThread(_tid); }


			/********************************************************
			 ** Functions to be used by the Pistachio backend only **
			 ********************************************************/

			/**
			 * Constructor
			 *
			 * Creates a L4 capability manually. This must not be called from
			 * generic code.
			 */
			Native_capability(Okl4::L4_ThreadId_t tid, long local_name)
			: _tid(tid), _local_name(local_name) { }

			/**
			 * Access raw capability data
			 */
			Okl4::L4_ThreadId_t tid() const { return _tid; };
	};

	typedef Okl4::L4_ThreadId_t Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
