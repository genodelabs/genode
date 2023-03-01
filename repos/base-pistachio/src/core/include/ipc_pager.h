/*
 * \brief  Pistachio pager support for Genode
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
#include <util/touch.h>

/* core includes */
#include <types.h>
#include <kip.h>
#include <mapping.h>

/* base-internal includes */
#include <base/internal/pistachio.h>

namespace Core { class Ipc_pager; }


class Core::Ipc_pager
{
	private:

		Pistachio::L4_ThreadId_t _last;       /* origin of last fault message   */
		Pistachio::L4_Word_t     _flags;      /* page-fault attributes          */
		addr_t                   _pf_addr;    /* page-fault address             */
		addr_t                   _pf_ip;      /* instruction pointer of faulter */
		Pistachio::L4_MapItem_t  _map_item;   /* page-fault answer              */

	protected:

		/**
		 * Wait for short-message (register) IPC -- pagefault
		 */
		void _wait();

		/**
		 * Send short flex page and
		 * wait for next short-message (register) IPC -- pagefault
		 */
		void _reply_and_wait();

	public:

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
		addr_t fault_ip() { return _pf_ip; }

		/**
		 * Request fault address of current page fault
		 */
		addr_t fault_addr() { return _pf_addr & ~3; }

		/**
		 * Set parameters for next reply
		 */
		void set_reply_mapping(Mapping const &mapping)
		{
			using namespace Pistachio;

			L4_Fpage_t fpage = L4_FpageLog2(mapping.src_addr, mapping.size_log2);

			fpage += mapping.writeable ? L4_FullyAccessible : L4_Readable;

			_map_item = L4_MapItem(fpage, mapping.dst_addr);
		}

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
		bool request_from_core() { return true; }

		/**
		 * Return badge for faulting thread
		 *
		 * As L4v4 has no server-defined badges for fault messages,
		 * we interpret the sender ID as badge.
		 */
		unsigned long badge() const { return _last.raw; }

		/**
		 * Return true if last fault was a write fault
		 */
		bool write_fault() const { return (_flags & 2); }

		/**
		 * Return true if last fault was a executable fault
		 */
		bool exec_fault() const { return false; }

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
