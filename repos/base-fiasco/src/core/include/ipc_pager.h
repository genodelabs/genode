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
#include <base/native_capability.h>
#include <util/touch.h>

/* base-internal includes */
#include <base/internal/fiasco_thread_helper.h>

/* core includes */
#include <util.h>
#include <mapping.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

namespace Core { class Ipc_pager; }


class Core::Ipc_pager
{
	private:

		Fiasco::l4_threadid_t _last          { };    /* origin of last fault message   */
		addr_t                _pf_addr       { 0 };  /* page-fault address             */
		addr_t                _pf_ip         { 0 };  /* instruction pointer of faulter */
		Mapping               _reply_mapping { };    /* page-fault answer              */

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

		bool exec_fault()  const { return false; }

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

#endif /* _CORE__INCLUDE__IPC_PAGER_H_ */
