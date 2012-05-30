/*
 * \brief  IPC backend for a Genode pager
 * \author Martin Stein
 * \date   2012-03-28
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_PAGER_H_
#define _INCLUDE__BASE__IPC_PAGER_H_

/* Genode includes */
#include <base/thread.h>
#include <base/ipc.h>
#include <base/native_types.h>
#include <kernel/log.h>

namespace Genode
{
	class Pager_object;

	/**
	 * Translation of a virtual page frame
	 */
	struct Mapping
	{
		addr_t   virt_address;
		addr_t   phys_address;
		bool     write_combined;
		unsigned size_log2;
		bool     writable;

		/**
		 * Construct valid mapping
		 */
		Mapping(addr_t const va, addr_t const pa, bool const wc,
		        unsigned const sl2 = MIN_MAPPING_SIZE_LOG2, bool w = 1)
		:
			virt_address(va), phys_address(pa), write_combined(wc),
			size_log2(sl2), writable(w)
		{ }

		/**
		 * Construct invalid mapping
		 */
		Mapping() : size_log2(0) { }

		/**
		 * Dummy, all data is available since construction
		 */
		void prepare_map_operation() { }

		/**
		 * Validation
		 */
		bool valid() { return size_log2 > 0; }
	};

	/**
	 * Message format for the acknowledgment of a resolved pagefault
	 */
	struct Pagefault_resolved
	{
		Native_thread_id const reply_dst;
		Pager_object * const   pager_object;
	};

	/**
	 * Special paging server class
	 */
	class Ipc_pager : public Native_capability
	{
		enum { VERBOSE = 1 };

		Pagefault _pagefault; /* data of lastly received pagefault */
		Mapping   _mapping; /* mapping to resolve last pagefault */

		public:

			/**
			 * Constructor
			 */
			Ipc_pager() :
				Native_capability(Genode::thread_get_my_native_id(), 0)
			{
				/* check if we can distinguish all message types */
				if (sizeof(Pagefault) == sizeof(Pagefault_resolved))
				{
					kernel_log() << __PRETTY_FUNCTION__
					             << ": Message types indiscernible\n";
					while (1) ;
				}
			}

			/**
			 * Wait for the next pagefault request
			 */
			void wait_for_fault();

			/**
			 * Resolve current pagefault and wait for a new one
			 */
			void resolve_and_wait_for_fault();

			/**
			 * Request instruction pointer of current page fault
			 */
			addr_t fault_ip() { return _pagefault.virt_ip; }

			/**
			 * Request fault address of current page fault
			 */
			addr_t fault_addr() { return _pagefault.virt_address; }

			/**
			 * Set parameters for next reply
			 */
			void set_reply_mapping(Mapping m) { _mapping = m; }

			/**
			 * Set destination for next reply
			 */
			void set_reply_dst(Native_capability pager_object) {
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			}

			/**
			 * Answer call without sending a flex-page mapping
			 *
			 * This function is used to acknowledge local calls from one of
			 * core's region-manager sessions.
			 */
			void acknowledge_wakeup() {
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			}

			/**
			 * Return thread ID of last faulter
			 */
			Native_thread_id last() const { return _pagefault.thread_id; }

			/**
			 * Return badge for faulting thread
			 */
			unsigned long badge() const { return _pagefault.thread_id; }

			/**
			 * Return true if last fault was a write fault
			 */
			bool is_write_fault() const { return _pagefault.write; }

			/**
			 * Return true if last fault was an exception
			 */
			bool is_exception() const
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
				return false;
			}
	};
}

#endif /* _INCLUDE__BASE__IPC_PAGER_H_ */

