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

#ifndef _CORE__INCLUDE__IPC_PAGER_H_
#define _CORE__INCLUDE__IPC_PAGER_H_

/* Genode includes */
#include <base/cache.h>
#include <base/ipc.h>
#include <base/stdint.h>
#include <base/printf.h>

/* NOVA includes */
#include <nova/syscalls.h>

namespace Genode {

	class Mapping
	{
		private:

			addr_t _dst_addr;
			addr_t _core_local_addr;
			Cache_attribute _attr;
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
				_attr(c), _size_log2(size_log2), _rw(rw)
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

			bool dma() { return _attr != CACHED; };
			bool write_combined() { return _attr == WRITE_COMBINED; };

			addr_t dst_addr() { return _dst_addr; }
	};


	class Ipc_pager
	{
		private:

			addr_t  _fault_ip;
			addr_t  _fault_addr;
			uint8_t _fault_type;

		public:

			/*
			 * Intel manual: 6.15 EXCEPTION AND INTERRUPT REFERENCE
			 *                    Interrupt 14—Page-Fault Exception (#PF)
			 */
			enum {
				ERR_I = 1 << 4,
				ERR_R = 1 << 3,
				ERR_U = 1 << 2,
				ERR_W = 1 << 1,
				ERR_P = 1 << 0,
			};

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
			void reply_and_wait_for_fault(unsigned sm = 0);

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
			bool write_fault() const { return _fault_type & ERR_W; }

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
