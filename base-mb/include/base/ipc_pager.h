/*
 * \brief  Dummy pager support for Genode
 * \author Norman Feske,
 *         Martin Stein
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_PAGER_H_
#define _INCLUDE__BASE__IPC_PAGER_H_

/* Genode includes */
#include <base/ipc.h>
#include <base/stdint.h>
#include <base/native_types.h>
#include <base/thread.h>

/* Kernel includes */
#include <kernel/config.h>
#include <kernel/syscalls.h>


namespace Genode {

	namespace Paging {

		typedef Kernel::Paging::Resolution Native_resolution;

		/**
		 * Used by Genode's IPC Pager and RM Session Component
		 */
		class Resolution : public Native_resolution{

			public:

				typedef Kernel::Paging::Physical_page Physical_page;
				typedef Kernel::Paging::Virtual_page  Virtual_page;

			private:

				enum {
					INVALID_SIZE       = Physical_page::INVALID_SIZE,
					NO_PROTECTION_ID   = 0,
					DEFAULT_SIZE_LOG2  = Kernel::DEFAULT_PAGE_SIZE_LOG2,
					DEFAULT_WRITEABLE  = true,
					DEFAULT_EXECUTABLE = true
				};

				bool _valid;

			public:

				::Genode::Native_page_size _native_size(unsigned const size_log2)
				{
					using namespace Kernel;
					using namespace Kernel::Paging;

					Physical_page::size_t s;
					return Physical_page::size_by_size_log2(s, size_log2) ?
					       Physical_page::INVALID_SIZE : s;
				}

				Native_page_permission _native_permission(bool const writeable,
				                                          bool const executable)
				{
					typedef Kernel::Paging::Physical_page Physical_page;

					if (writeable){
						if (executable) return Physical_page::RWX;
						else            return Physical_page::RW;}
					else{
						if (executable) return Physical_page::RX;
						else            return Physical_page::R;}
				}

				Resolution(addr_t   virtual_page_address,
				           addr_t   physical_page_address,
				           bool     write_combined,
				           unsigned size_log2 = DEFAULT_SIZE_LOG2,
				           bool     writeable = DEFAULT_WRITEABLE)
				: _valid(true)
				{
					virtual_page = Virtual_page(virtual_page_address,
					                            NO_PROTECTION_ID);

					physical_page = Physical_page(physical_page_address,
					                              _native_size(size_log2),
					                              _native_permission(writeable,
					                                                 DEFAULT_EXECUTABLE));
				}

				Resolution() : _valid(false) { }

				void prepare_map_operation() { }

				inline bool valid() { return _valid; }
		};
	}

	typedef Paging::Resolution Mapping;

	/**
	 * Special paging server class
	 */
	class Ipc_pager : public Native_capability
	{
		typedef Kernel::Paging::Request Request;

		Mapping _mapping;
		Request _request;

		public:

			/**
			 * Constructor
			 */
			Ipc_pager()
			: Native_capability(Genode::my_thread_id(), 0)
			{
				_request.source.tid = 0;
			}

			/**
			 * Wait for a new fault received as short message IPC
			 */
			void wait_for_fault();

			/**
			 * Reply current fault and wait for a new one
			 *
			 * Send short flex page and wait for next short-message (register)
			 * IPC -- pagefault
			 */
			void reply_and_wait_for_fault();

			bool resolved();

			/**
			 * Request instruction pointer of current fault
			 */
			addr_t fault_ip() { return _request.source.ip; }

			/**
			 * Request fault address of current page fault
			 */
			addr_t fault_addr() { return _request.virtual_page.address(); }

			/**
			 * Set parameters for next reply
			 */
			inline void set_reply_mapping(Mapping m) { _mapping=m; }

			/**
			 * Set destination for next reply
			 */
			inline void set_reply_dst(Native_capability pager_object) { }

			/**
			 * Answer call without sending a flex-page mapping
			 *
			 * This function is used to acknowledge local calls from one of
			 * core's region-manager sessions.
			 */
			inline void acknowledge_wakeup()
			{
				Kernel::thread_wake(_request.source.tid);
			}

			/**
			 * Return thread ID of last faulter
			 */
			inline Native_thread_id last() const { return _request.source.tid; }

			/**
			 * Return badge for faulting thread
			 */
			inline unsigned long badge() const { return _request.source.tid; }

			/**
			 * Was last fault a write fault?
			 */
			bool is_write_fault() const 
			{
				return _request.access==Kernel::Paging::Request::RW ||
				       _request.access==Kernel::Paging::Request::RWX; 
			}

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
