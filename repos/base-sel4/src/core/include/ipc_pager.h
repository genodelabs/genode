/*
 * \brief  Pager support for Genode on seL4
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IPC_PAGER_H_
#define _CORE__INCLUDE__IPC_PAGER_H_

/* Genode includes */
#include <base/ipc.h>

/* core includes */
#include <mapping.h>

namespace Core { class Ipc_pager; }


class Core::Ipc_pager : public Native_capability
{
	private:

		addr_t _badge      = 0;    /* faulted badge of thread        */
		addr_t _reply_sel  = 0;    /* selector to save reply cap     */
		addr_t _pf_addr    = 0;    /* page-fault address             */
		addr_t _pf_ip      = 0;    /* instruction pointer of faulter */
		bool _exception  = false;  /* true on non page fault         */
		bool _pf_write   = false;  /* true on write fault            */
		bool _pf_exec    = false;  /* true on exec  fault            */
		bool _pf_align   = false;  /* true on unaligned fault        */

		Mapping _reply_mapping { };

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
		 */
		void reply_and_wait_for_fault();

		/**
		 * Request instruction pointer of current page fault
		 */
		addr_t fault_ip() { return _pf_ip; }

		/**
		 * Request fault address of current page fault
		 */
		addr_t fault_addr() { return _pf_addr; }

		/**
		 * Set parameters for next reply
		 */
		void set_reply_mapping(Mapping m) { _reply_mapping = m; }

		/**
		 * Set destination for next reply
		 */
		void reply_save_caller(addr_t sel) { _reply_sel = sel; }

		/**
		 * Return badge for faulting thread
		 */
		unsigned long badge() const { return _badge; }

		/**
		 * Return true if page fault was a write fault
		 */
		bool write_fault() const { return _pf_write; }

		/**
		 * Return true if page fault was on non-executable memory
		 */
		bool exec_fault() const { return _pf_exec; }

		/**
		 * Return true if page fault was due to unaligned memory access
		 */
		bool align_fault() const { return _pf_align; }

		/**
		 * Install memory mapping after pager code executed.
		 */
		bool install_mapping();

		/**
		 * Return true if last fault was an exception
		 */
		bool exception() const { return _exception; }
};

#endif /* _CORE__INCLUDE__IPC_PAGER_H_ */
