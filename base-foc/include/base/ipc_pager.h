/*
 * \brief  Fiasco.OC pager support
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-06-14
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
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
#include <base/thread_state.h>
#include <util/touch.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode {

	class Mapping
	{
		private:

			addr_t   _dst_addr;
			addr_t   _src_addr;
			bool     _write_combined;
			unsigned _log2size;
			bool     _rw;
			bool     _grant;

		public:

			/**
			 * Constructor
			 */
			Mapping(addr_t dst_addr, addr_t src_addr,
			        bool write_combined, unsigned l2size = L4_LOG2_PAGESIZE,
			        bool rw = true, bool grant = false)
			: _dst_addr(dst_addr), _src_addr(src_addr),
			  _write_combined(write_combined), _log2size(l2size),
			  _rw(rw), _grant(grant) { }

			/**
			 * Construct invalid flexpage
			 */
			Mapping() : _dst_addr(0), _src_addr(0), _write_combined(false),
			            _log2size(0), _rw(false), _grant(false) { }

			Fiasco::l4_umword_t dst_addr() const { return _dst_addr; }
			bool                grant()    const { return _grant;    }

			Fiasco::l4_fpage_t  fpage()    const
			{
				// TODO: write combined
				//if (write_combined)
				//	_fpage.fp.cache = Fiasco::L4_FPAGE_BUFFERABLE;

				unsigned char rights = _rw ? Fiasco::L4_FPAGE_RW : Fiasco::L4_FPAGE_RO;
				return Fiasco::l4_fpage(_src_addr, _log2size, rights);
			}

			bool write_combined() const { return _write_combined; }

			/**
			 * Prepare map operation
			 *
			 * On Fiasco, we need to map a page locally to be able to map it to
			 * another address space.
			 */
			void prepare_map_operation()
			{
				size_t mapping_size = 1 << _log2size;
				for (addr_t i = 0; i < mapping_size; i += L4_PAGESIZE) {
					if (_rw)
						touch_read_write((unsigned char volatile *)(_src_addr + i));
					else
						touch_read((unsigned char const volatile *)(_src_addr + i));
				}
			}
	};


	/**
	 * Special paging server class
	 */
	class Ipc_pager : public Native_capability
	{
		public:

			enum Msg_type { PAGEFAULT, WAKE_UP, PAUSE, EXCEPTION };

		private:

			Native_thread         _last;           /* origin of last fault     */
			addr_t                _pf_addr;        /* page-fault address       */
			addr_t                _pf_ip;          /* ip of faulter            */
			Mapping               _reply_mapping;  /* page-fault answer        */
			unsigned long         _badge;          /* badge of faulting thread */
			Fiasco::l4_msgtag_t   _tag;            /* receive message tag      */
			Fiasco::l4_exc_regs_t _regs;           /* exception registers      */
			Msg_type              _type;

			void _parse_msg_type(void);
			void _parse_exception(void);
			void _parse_pagefault(void);
			void _parse(unsigned long label);

		public:

			/**
			 * Constructor
			 */
			Ipc_pager();

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
			void set_reply_dst(Native_thread t) {
				_last = t; }

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
			Native_thread last() const { return _last; }

			/**
			 * Return badge for faulting thread
			 */
			unsigned long badge() { return _badge; }

			bool is_write_fault() const { return (_pf_addr & 2); }

			/**
			 * Return true if last fault was an exception
			 */
			bool is_exception() const
			{
				return _type == Ipc_pager::EXCEPTION;
			}

			/**
			 * Return the type of ipc we received at last.
			 */
			Msg_type msg_type() { return _type; };

			/**
			 * Copy the exception registers from the last exception
			 * to the given thread_state object.
			 */
			void copy_regs(Thread_state *state);
	};
}

#endif /* _INCLUDE__BASE__IPC_PAGER_H_ */
