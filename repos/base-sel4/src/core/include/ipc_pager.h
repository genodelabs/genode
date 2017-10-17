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

#include <base/cache.h>
#include <base/ipc.h>
#include <base/stdint.h>

namespace Genode {
	class Mapping;
	class Ipc_pager;
}


class Genode::Mapping
{
	friend class Ipc_pager;

	private:

		addr_t          _from_phys_addr;
		addr_t          _to_virt_addr;
		Cache_attribute _attr;
		size_t          _num_pages;
		addr_t          _fault_type = { 0 };
		bool            _writeable  = { false };
		bool            _executable = { false };

		enum { PAGE_SIZE_LOG2 = 12 };

	public:

		/**
		 * Constructor
		 */
		Mapping(addr_t dst_addr, addr_t src_addr,
		        Cache_attribute const cacheability, bool io_mem,
		        unsigned l2size, bool rw, bool executable)
		:
			_from_phys_addr(src_addr),
			_to_virt_addr(dst_addr),
			_attr(cacheability),
			_num_pages(1 << (l2size - PAGE_SIZE_LOG2)),
			_writeable(rw), _executable(executable)
		{ }

		/**
		 * Construct invalid mapping
		 */
		Mapping() : _num_pages(0) { }

		/**
		 * Prepare map operation
		 *
		 * No preparations are needed on seL4 because all mapping
		 * originate from the physical address space.
		 */
		void prepare_map_operation() { }

		addr_t from_phys() const { return _from_phys_addr; }
		addr_t to_virt()   const { return _to_virt_addr; }
		size_t num_pages() const { return _num_pages; }
		bool   writeable() const { return _writeable; }
		bool   executable() const { return _executable; }
		Cache_attribute cacheability() const { return _attr; }
		addr_t fault_type() const { return _fault_type; }
};


/**
 * Special paging server class
 */
class Genode::Ipc_pager : public Native_capability
{
	private:

		addr_t           _badge;     /* faulted badge of thread        */
		addr_t           _reply_sel; /* selector to save reply cap     */
		addr_t           _pf_addr;   /* page-fault address             */
		addr_t           _pf_ip;     /* instruction pointer of faulter */
		addr_t           _fault_type; /* type of fault */
		bool             _pf_write;  /* true on write fault            */
		bool             _pf_exec;   /* true on exec  fault            */

		Mapping _reply_mapping;

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
		void set_reply_mapping(Mapping m)
		{
			_reply_mapping = m;
			_reply_mapping._fault_type = _fault_type;
		}

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
};

#endif /* _CORE__INCLUDE__IPC_PAGER_H_ */
