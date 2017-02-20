/*
 * \brief  Fiasco pager support
 * \author Christian Helmuth
 * \date   2006-06-14
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IPC_PAGER_H_
#define _CORE__INCLUDE__IPC_PAGER_H_

/* Genode includes */
#include <base/cache.h>
#include <base/ipc.h>
#include <base/stdint.h>
#include <base/native_capability.h>
#include <util/touch.h>

/* base-internal includes */
#include <base/internal/fiasco_thread_helper.h>

/* core includes */
#include <util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode {

	class Mapping
	{
		private:

			addr_t             _dst_addr;
			Fiasco::l4_fpage_t _fpage;

		public:

			/**
			 * Constructor
			 */
			Mapping(addr_t dst_addr, addr_t src_addr,
			        Cache_attribute cacheability, bool io_mem,
			        unsigned l2size = L4_LOG2_PAGESIZE,
			        bool rw = true, bool grant = false)
			:
				_dst_addr(dst_addr),
				_fpage(Fiasco::l4_fpage(src_addr, l2size, rw, grant))
			{
				if (cacheability == WRITE_COMBINED)
					_fpage.fp.cache = Fiasco::L4_FPAGE_BUFFERABLE;
			}

			/**
			 * Construct invalid flexpage
			 */
			Mapping() : _dst_addr(0), _fpage(Fiasco::l4_fpage(0, 0, 0, 0)) { }

			Fiasco::l4_umword_t dst_addr() const { return _dst_addr; }
			Fiasco::l4_fpage_t  fpage()    const { return _fpage; }

			/**
			 * Prepare map operation
			 *
			 * On Fiasco, we need to map a page locally to be able to map it to
			 * another address space.
			 */
			void prepare_map_operation()
			{
				addr_t core_local_addr = _fpage.fp.page << 12;
				size_t mapping_size    = 1 << _fpage.fp.size;

				for (addr_t i = 0; i < mapping_size; i += L4_PAGESIZE) {
					if (_fpage.fp.write)
						touch_read_write((unsigned char volatile *)(core_local_addr + i));
					else
						touch_read((unsigned char const volatile *)(core_local_addr + i));
				}
			}
	};


	/**
	 * Special paging server class
	 */
	class Ipc_pager
	{
		private:

			Fiasco::l4_threadid_t _last;           /* origin of last fault message   */
			addr_t                _pf_addr;        /* page-fault address             */
			addr_t                _pf_ip;          /* instruction pointer of faulter */
			Mapping               _reply_mapping;  /* page-fault answer              */

		public:

			/**
			 * Wait for a new page fault received as short message IPC
			 */
			void wait_for_fault();

			/**
			 * Reply current page-fault and wait for a new one
			 *
			 * Send short flex page and wait for next short-message (register)
			 * IPC -- pagefault
			 */
			void reply_and_wait_for_fault();

			/**
			 * Request instruction pointer of current page fault
			 */
			addr_t fault_ip() { return _pf_ip; }

			/**
			 * Request fault address of current page fault
			 */
			addr_t fault_addr() { return _pf_addr & ~3; }

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
			 * Returns true if the last request was send from a core thread
			 */
			bool request_from_core()
			{
				enum { CORE_TASK_ID = 4 };
				return _last.id.task == CORE_TASK_ID;
			}

			/**
			 * Return badge for faulting thread
			 *
			 * As Fiasco has no server-defined badges for page-fault messages, we
			 * interpret the sender ID as badge.
			 */
			unsigned long badge() const {
				return convert_native_thread_id_to_badge(_last); }

			bool write_fault() const { return (_pf_addr & 2); }

			/**
			 * Return true if last fault was an exception
			 */
			bool exception() const
			{
				/*
				 * Reflection of exceptions is not supported on this platform.
				 */
				return false;
			}
	};
}

#endif /* _CORE__INCLUDE__IPC_PAGER_H_ */
