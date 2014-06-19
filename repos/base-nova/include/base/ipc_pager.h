/*
 * \brief  Low-level page-fault handling
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_PAGER_H_
#define _INCLUDE__BASE__IPC_PAGER_H_

/* Genode includes */
#include <base/cache.h>
#include <base/ipc.h>
#include <base/stdint.h>
#include <base/native_types.h>
#include <base/printf.h>

/* NOVA includes */
#include <nova/syscalls.h>

namespace Genode {

	class Mapping
	{
		private:

			addr_t _dst_addr;
			addr_t _core_local_addr;
			bool   _write_combined;
			size_t _size_log2;
			bool   _rw;

			enum { PAGE_SIZE_LOG2 = 12 };

		public:

			/**
			 * Constructor
			 */
			Mapping(addr_t dst_addr, addr_t map_addr,
			        Cache_attribute c, bool io_mem,
			        unsigned size_log2 = PAGE_SIZE_LOG2,
			        bool rw = true)
			:
				_dst_addr(dst_addr), _core_local_addr(map_addr),
				_write_combined(c != CACHED), _size_log2(size_log2),
				_rw(rw)
			{ }

			/**
			 * Construct invalid mapping
			 */
			Mapping() : _size_log2(0) { }

			void prepare_map_operation() { }

			Nova::Mem_crd mem_crd()
			{
				return Nova::Mem_crd(_core_local_addr >> PAGE_SIZE_LOG2,
				                     _size_log2 - PAGE_SIZE_LOG2,
				                     Nova::Rights(true, _rw, true));
			}

			bool  write_combined() { return _write_combined; };

			addr_t dst_addr() { return _dst_addr; }
	};


	class Ipc_pager
	{
		private:

			/**
			 * Page-fault type
			 */
			enum Pf_type {
				TYPE_READ  = 0x4,
				TYPE_WRITE = 0x2,
				TYPE_EXEC  = 0x1,
			};

			addr_t  _fault_ip;
			addr_t  _fault_addr;
			Pf_type _fault_type;

		public:

			/**
			 * Wait for page-fault info
			 *
			 * After returning from this call, 'fault_ip' and 'fault_addr'
			 * have a defined state.
			 */
			void wait_for_fault();

			/**
			 * Answer current page fault
			 */
			void reply_and_wait_for_fault();

			/**
			 * Request instruction pointer of current fault
			 */
			addr_t fault_ip() { return _fault_ip; }

			/**
			 * Request page-fault address of current fault
			 */
			addr_t fault_addr() { return _fault_addr; }

			/**
			 * Set page-fault reply parameters
			 */
			void set_reply_mapping(Mapping m);

			/**
			 * Return true if fault was a write fault
			 */
			bool is_write_fault() const { return _fault_type == TYPE_WRITE; }

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
