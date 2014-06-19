/*
 * \brief  Dummy pager support for Genode
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_PAGER_H_
#define _INCLUDE__BASE__IPC_PAGER_H_

#include <base/cache.h>
#include <base/ipc.h>
#include <base/stdint.h>
#include <base/native_types.h>

namespace Genode {

	class Mapping
	{
		public:

			/**
			 * Constructor
			 */
			Mapping(addr_t dst_addr, addr_t src_addr,
			        Cache_attribute,  bool io_mem,
			        unsigned l2size = 12, bool rw = true) { }

			/**
			 * Construct invalid mapping
			 */
			Mapping() { }

			/**
			 * Prepare map operation
			 */
			void prepare_map_operation() { }
	};


	/**
	 * Special paging server class
	 */
	class Ipc_pager : public Native_capability
	{
		protected:

			/**
			 * Wait for short-message (register) IPC -- pagefault
			 */
			void _wait() { }

			/**
			 * Send short flex page and
			 * wait for next short-message (register) IPC -- pagefault
			 */
			void _reply_and_wait() { }

		public:

			/**
			 * Constructor
			 */
			Ipc_pager() { }

			/**
			 * Wait for a new fault received as short message IPC
			 */
			void wait_for_fault() { }

			/**
			 * Reply current page-fault and wait for a new one
			 *
			 * Send short flex page and wait for next short-message (register)
			 * IPC -- fault
			 */
			void reply_and_wait_for_fault() { }

			/**
			 * Request instruction pointer of current page fault
			 */
			addr_t fault_ip() { return 0; }

			/**
			 * Request fault address of current page fault
			 */
			addr_t fault_addr() { return 0; }

			/**
			 * Set parameters for next reply
			 */
			void set_reply_mapping(Mapping m) { }

			/**
			 * Set destination for next reply
			 */
			void set_reply_dst(Native_capability pager_object) { }

			/**
			 * Answer call without sending a flex-page mapping
			 *
			 * This function is used to acknowledge local calls from one of
			 * core's region-manager sessions.
			 */
			void acknowledge_wakeup() { }

			/**
			 * Return thread ID of last faulter
			 */
			Native_thread_id last() const { return 0; }

			/**
			 * Return badge for faulting thread
			 */
			unsigned long badge() const { return 0; }

			/**
			 * Return true if last fault was a write fault
			 */
			bool is_write_fault() const { return false; }

			/**
			 * Return true if last fault was an exception
			 */
			bool is_exception() const
			{
				/*
				 * Reflection of exceptions is not supported on this platform.
				 */
				return false;
			}
	};
}

#endif /* _INCLUDE__BASE__IPC_PAGER_H_ */
