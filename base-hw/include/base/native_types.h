/*
 * \brief  Basic Genode types
 * \author Martin Stein
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__NATIVE_TYPES_H_
#define _BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/interface.h>
#include <base/native_capability.h>
#include <base/stdint.h>

namespace Genode
{
	class Platform_thread;
	class Tlb;

	typedef unsigned Native_thread_id;

	struct Native_thread
	{
		Platform_thread  * platform_thread;
		Native_thread_id   thread_id;
	};

	typedef int Native_connection_state;

	/* FIXME needs to be MMU dependent */
	enum { MIN_MAPPING_SIZE_LOG2 = 12 };

	/**
	 * Return kernel thread-name of the caller
	 */
	Native_thread_id thread_get_my_native_id();

	/**
	 * Return an invalid kernel thread-name
	 */
	inline Native_thread_id thread_invalid_id() { return 0; }

	/**
	 * Message that is communicated between user threads
	 */
	struct Ipc_msg
	{
		size_t  size;
		uint8_t data[];
	};

	/**
	 * Message that is communicated from a thread creator to the new thread
	 */
	class Startup_msg;

	/**
	 * Memory region that is exclusive to every thread and known by the kernel
	 */
	struct Native_utcb;

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
	 * Coherent address region
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

class Genode::Startup_msg
{
	private:

		Native_thread_id _thread_id;

	public:

		/**
		 * Set-up valid startup message
		 *
		 * \param thread_id  kernel name of the thread that is started
		 */
		void init(Native_thread_id const thread_id) { _thread_id = thread_id; }


		/***************
		 ** Accessors **
		 ***************/

		Native_thread_id thread_id() const { return _thread_id; }
};

struct Genode::Native_utcb
{
	union {
		uint8_t     data[1 << MIN_MAPPING_SIZE_LOG2];
		Ipc_msg     ipc_msg;
		Startup_msg startup_msg;
	};

	void call_await_request_msg(void * & buf_base, size_t & buf_size)
	{
		buf_base = base();
		buf_size = size();
	}

	void call_send_request_msg(void * & msg_base, size_t & msg_size,
	                           void * & buf_base, size_t & buf_size)
	{
		msg_base = ipc_msg_base();
		msg_size = ipc_msg_size();
		buf_base = base();
		buf_size = size();
	}

	void call_send_reply_msg(void * & msg_base, size_t & msg_size)
	{
		msg_base = ipc_msg_base();
		msg_size = ipc_msg_size();
	}

	size_t size() { return sizeof(data) / sizeof(data[0]); }
	void * base() { return &data; }
	addr_t top() { return (addr_t)base() + size(); }
	void * ipc_msg_base() { return &ipc_msg; }
	size_t ipc_msg_size() { return ipc_msg_header_size() + ipc_msg.size; }
	size_t ipc_msg_max_size() { return top() - (addr_t)&ipc_msg; }
	size_t ipc_msg_header_size() { return (addr_t)ipc_msg.data - (addr_t)&ipc_msg; }
};

#endif /* _BASE__NATIVE_TYPES_H_ */

