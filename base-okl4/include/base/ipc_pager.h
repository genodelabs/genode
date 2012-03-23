/*
 * \brief  OKL4 pager support for Genode
 * \author Norman Feske
 * \date   2009-03-31
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_PAGER_H_
#define _INCLUDE__BASE__IPC_PAGER_H_

#include <base/ipc.h>
#include <base/stdint.h>
#include <base/native_types.h>

namespace Okl4 { extern "C" {
#include <l4/types.h>
#include <l4/map.h>
#include <l4/space.h>
} }

namespace Genode {

	class Mapping
	{
		private:

			addr_t              _phys_addr;
			Okl4::L4_Fpage_t    _fpage;
			Okl4::L4_PhysDesc_t _phys_desc;

		public:

			/**
			 * Constructor
			 */
			Mapping(addr_t dst_addr, addr_t src_addr,
			        bool write_combined, unsigned l2size = 12, bool rw = true);

			/**
			 * Construct invalid mapping
			 */
			Mapping();

			/**
			 * Return flexpage describing the virtual destination address
			 */
			Okl4::L4_Fpage_t fpage() const { return _fpage; }

			/**
			 * Return physical-memory descriptor describing the source location
			 */
			Okl4::L4_PhysDesc_t phys_desc() const { return _phys_desc; }

			/**
			 * Prepare map operation
			 *
			 * On OKL4, we do not need to map a page core-locally to be able to
			 * map it into another address space. Therefore, we can leave this
			 * function blank.
			 */
			void prepare_map_operation() { }
	};


	/**
	 * Special paging server class
	 */
	class Ipc_pager : public Native_capability
	{
		private:

			Okl4::L4_MsgTag_t   _faulter_tag;   /* fault flags                    */
			Okl4::L4_ThreadId_t _last;          /* faulted thread                 */
			Okl4::L4_Word_t     _last_space;    /* space of faulted thread        */
			Okl4::L4_Word_t     _fault_addr;    /* page-fault address             */
			Okl4::L4_Word_t     _fault_ip;      /* instruction pointer of faulter */
			Mapping             _reply_mapping; /* page-fault answer              */

		protected:

			/**
			 * Wait for short-message (register) IPC -- fault
			 */
			void _wait();

			/**
			 * Send short flex page and
			 * wait for next short-message (register) IPC -- fault
			 */
			void _reply_and_wait();

		public:

			/**
			 * Constructor
			 */
			Ipc_pager();

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

			/**
			 * Request instruction pointer of current fault
			 */
			addr_t fault_ip() { return _fault_ip; }

			/**
			 * Request fault address of current fault
			 */
			addr_t fault_addr() { return _fault_addr & ~3; }

			/**
			 * Set parameters for next reply
			 */
			void set_reply_mapping(Mapping m) { _reply_mapping = m; }

			/**
			 * Set destination for next reply
			 */
			void set_reply_dst(Native_capability pager_object) {
				_last.raw = pager_object.local_name(); }

			/**
			 * Answer call without sending a flex-page mapping
			 *
			 * This function is used to acknowledge local calls from one of
			 * core's region-manager sessions.
			 */
			void acknowledge_wakeup();

			/**
			 * Return thread ID of last faulter
			 */
			Native_thread_id last() const { return _last; }

			/**
			 * Return address space where the last page fault occurred
			 */
			unsigned long last_space() const { return _last_space; }

			/**
			 * Return badge for faulting thread
			 *
			 * Because OKL4 has no server-defined badges for fault messages, we
			 * interpret the sender ID as badge.
			 */
			unsigned long badge() const { return _last.raw; }

			/**
			 * Return true if last fault was a write fault
			 */
			bool is_write_fault() const { return L4_Label(_faulter_tag) & 2; }

			/**
			 * Return true if last fault was an exception
			 */
			bool is_exception() const
			{
				/*
				 * A page-fault message has one of the op bits (lower 3 bits of the
				 * label) set. If those bits are zero, we got an exception message.
				 * If the label is zero, we got an IPC wakeup message from within
				 * core.
				 */
				return L4_Label(_faulter_tag) && (L4_Label(_faulter_tag) & 0xf) == 0;
			}
	};
}

#endif /* _INCLUDE__BASE__IPC_PAGER_H_ */
