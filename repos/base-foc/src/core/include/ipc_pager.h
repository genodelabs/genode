/*
 * \brief  Fiasco.OC pager support
 * \author Christian Helmuth
 * \author Stefan Kalkowski
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
#include <base/ipc.h>
#include <base/stdint.h>
#include <base/native_capability.h>
#include <base/thread_state.h>
#include <util/touch.h>
#include <foc/native_capability.h>
#include <foc/thread_state.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* core includes */
#include <mapping.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

namespace Core { class Ipc_pager; }


class Core::Ipc_pager : public Native_capability
{
	public:

		enum Msg_type { PAGEFAULT, WAKE_UP, PAUSE, EXCEPTION };

	private:

		Native_thread      _last    { };        /* origin of last fault     */
		addr_t             _pf_addr { 0 };      /* page-fault address       */
		addr_t             _pf_ip   { 0 };      /* ip of faulter            */
		Mapping            _reply_mapping { };  /* page-fault answer        */
		unsigned long      _badge;              /* badge of faulting thread */
		Foc::l4_msgtag_t   _tag  { };           /* receive message tag      */
		Foc::l4_exc_regs_t _regs { };           /* exception registers      */
		Msg_type           _type { PAGEFAULT };

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
		 * Request fault address of current page fault. Lower 3 bits are used
		 * to encode error codes.
		 */
		addr_t fault_addr() { return _pf_addr & ~7UL; }

		/**
		 * Set parameters for next reply
		 */
		void set_reply_mapping(Mapping m) { _reply_mapping = m; }

		/**
		 * Set destination for next reply
		 */
		void set_reply_dst(Native_thread t) { _last = t; }

		/**
		 * Answer call without sending a flex-page mapping
		 *
		 * This function is used to acknowledge local calls from one of
		 * core's region-manager sessions.
		 */
		void acknowledge_wakeup();

		/**
		 * Reply to an exception IPC
		 */
		void acknowledge_exception();

		/**
		 * Return thread ID of last faulter
		 */
		Native_thread last() const { return _last; }

		/**
		 * Return badge for faulting thread
		 */
		unsigned long badge() { return _badge; }

		/**
		 * Return true if fault was a write fault
		 */
		bool write_fault() const { return (_pf_addr & 2); }

		/**
		 * Return true if fault was caused by execution on non executable memory
		 */
		bool exec_fault() const;

		/**
		 * Return true if last fault was an exception
		 */

		bool exception() const
		{
			return _type == Ipc_pager::EXCEPTION;
		}

		/**
		 * Return the type of ipc we received at last.
		 */
		Msg_type msg_type() { return _type; };

		/**
		 * Copy the exception registers from the last exception
		 * to the given Thread_state object.
		 */
		void get_regs(Foc_thread_state &state) const;

		/*
		 * Copy the exception reply registers from the given
		 * Thread_state object
		 */
		void set_regs(Foc_thread_state const &state);
};

#endif /* _CORE__INCLUDE__IPC_PAGER_H_ */
