/*
 * \brief  Dummy definitions for native types used for compiling unit tests
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

#include <kernel/types.h>

namespace Genode {

	typedef Kernel::Thread_id     Native_thread_id;
	typedef Native_thread_id      Native_thread;

	typedef Kernel::Protection_id Native_process_id;

	typedef Kernel::Utcb_unaligned                     Native_utcb;
	typedef Kernel::Paging::Physical_page::Permissions Native_page_permission;
	typedef Kernel::Paging::Physical_page::size_t      Native_page_size;

	Native_thread_id my_thread_id();

	class Native_capability
	{
		private:

			Native_thread_id  _tid;
			long              _local_name;

		public:

			Native_capability() : _tid(0), _local_name(0) { }

			Native_capability(Native_thread_id tid, long local_name)
			: _tid(tid), _local_name(local_name) { }

			bool valid() const { return _tid!=0; }

			int local_name() const { return _local_name; }

			int dst() const { return (int)_tid; }

			Native_thread_id tid() const { return _tid; }
	};

	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */




