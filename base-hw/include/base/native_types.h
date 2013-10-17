/*
 * \brief  Platform specific basic Genode types
 * \author Martin Stein
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/syscalls.h>
#include <base/native_capability.h>
#include <base/stdint.h>

namespace Genode
{
	class Platform_thread;
	class Tlb;

	typedef unsigned Native_thread_id;

	struct Native_thread
	{
		Native_thread_id  tid;
		Platform_thread  *pt;
	};

	typedef int Native_connection_state;

	/* FIXME needs to be MMU dependent */
	enum { MIN_MAPPING_SIZE_LOG2 = 12 };

	/**
	 * Get kernel-object identifier of the current thread
	 */
	inline Native_thread_id thread_get_my_native_id()
	{ return Kernel::current_thread_id(); }

	/**
	 * Get the thread ID, wich is handled as invalid by the kernel
	 */
	inline Native_thread_id thread_invalid_id() { return 0; }

	/**
	 * Describes a pagefault
	 */
	struct Pagefault
	{
		unsigned thread_id;    /* thread ID of the faulter */
		Tlb *    tlb;          /* TLB to wich the faulter is assigned */
		addr_t   virt_ip;      /* the faulters virtual instruction pointer */
		addr_t   virt_address; /* virtual fault address */
		bool     write;        /* write access attempted at fault? */

		/**
		 * Placement new operator
		 */
		void * operator new (size_t, void * p) { return p; }

		/**
		 * Construct invalid pagefault
		 */
		Pagefault() : thread_id(0) { }

		/**
		 * Construct valid pagefault
		 */
		Pagefault(unsigned const tid, Tlb * const tlb,
		          addr_t const vip, addr_t const va, bool const w)
		:
			thread_id(tid), tlb(tlb), virt_ip(vip),
			virt_address(va), write(w)
		{ }

		/**
		 * Validation
		 */
		bool valid() const { return thread_id != 0; }
	};

	/**
	 * Types of synchronously communicated messages
	 */
	struct Msg_type
	{
		enum Id {
			INVALID = 0,
			IPC     = 1,
		};
	};

	/**
	 * Message that is communicated synchronously
	 */
	struct Msg
	{
		Msg_type::Id type;
		uint8_t      data[];
	};

	/**
	 * Message that is communicated between user threads
	 */
	struct Ipc_msg : Msg
	{
		size_t  size;
		uint8_t data[];
	};

	/**
	 * Describes a userland-thread-context region
	 */
	struct Native_utcb
	{
		union {
			uint8_t data[1 << MIN_MAPPING_SIZE_LOG2];
			Msg     msg;
			Ipc_msg ipc_msg;
		};

		/**
		 * Get the base of the UTCB region
		 */
		void * base() { return data; }

		/**
		 * Get the size of the UTCB region
		 */
		size_t size() { return sizeof(data) / sizeof(data[0]); }

		/**
		 * Get the top of the UTCB region
		 */
		addr_t top()  { return (addr_t)data + size(); }

		/**
		 * Maximum size of an IPC message that can be held by the UTCB
		 */
		size_t ipc_msg_max_size() { return top() - (addr_t)ipc_msg.data; }
	};

	struct Cap_dst_policy
	{
		typedef Native_thread_id Dst;

		/**
		 * Validate capability destination
		 */
		static bool valid(Dst pt) { return pt != 0; }

		/**
		 * Get invalid capability destination
		 */
		static Dst invalid() { return 0; }

		/**
		 * Copy capability 'src' to a given memory destination 'dst'
		 */
		static void
		copy(void * dst, Native_capability_tpl<Cap_dst_policy> * src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	/**
	 * A coherent address region
	 */
	struct Native_region
	{
		addr_t base;
		size_t size;
	};

	struct Native_config
	{
		/**
		 * Thread-context area configuration.
		 */
		static addr_t context_area_virtual_base() { return 0x40000000UL; }
		static addr_t context_area_virtual_size() { return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static addr_t context_virtual_size() { return 0x00100000UL; }
	};

	struct Native_pd_args { };
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */

