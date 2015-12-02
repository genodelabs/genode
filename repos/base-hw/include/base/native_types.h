/*
 * \brief  Basic Genode types
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__NATIVE_TYPES_H_
#define _BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/log.h>
#include <base/native_capability.h>
#include <base/ipc_msgbuf.h>

namespace Genode
{
	class Platform_thread;
	class Native_thread;

	using Native_thread_id = Kernel::capid_t;

	typedef int Native_connection_state;

	/**
	 * Information that a thread creator hands out to a new thread
	 */
	class Start_info;

	/**
	 * Coherent address region
	 */
	struct Native_region;

	struct Native_config;

	struct Native_pd_args { };

	/**
	 * Get the the minimal supported page-size log 2
	 */
	constexpr size_t get_page_size_log2() { return 12; }

	/**
	 * Get the the minimal supported page-size
	 */
	constexpr size_t get_page_size() { return 1 << get_page_size_log2(); }

	/**
	 * Memory region that is exclusive to every thread and known by the kernel
	 */
	class Native_utcb;
}


struct Genode::Native_thread
{
	Platform_thread  * platform_thread;
	Native_capability  cap;
};


/**
 * Coherent address region
 */
struct Genode::Native_region
{
	addr_t base;
	size_t size;
};


struct Genode::Native_config
{
	/**
	 * Thread-context area configuration.
	 */
	static constexpr addr_t context_area_virtual_base() {
		return 0xe0000000UL; }
	static constexpr addr_t context_area_virtual_size() {
		return 0x10000000UL; }

	/**
	 * Size of virtual address region holding the context of one thread
	 */
	static constexpr addr_t context_virtual_size() { return 0x00100000UL; }
};


class Genode::Native_utcb
{
	public:

		enum { MAX_CAP_ARGS = Msgbuf_base::MAX_CAP_ARGS};

		enum Offsets { THREAD_MYSELF, PARENT, UTCB_DATASPACE };

	private:

		Kernel::capid_t _caps[MAX_CAP_ARGS]; /* capability buffer  */
		size_t          _cap_cnt = 0;        /* capability counter */
		size_t          _size    = 0;        /* bytes to transfer  */
		uint8_t         _buf[get_page_size() - sizeof(_caps) -
		                     sizeof(_cap_cnt) - sizeof(_size)];

	public:

		Native_utcb& operator= (const Native_utcb &o)
		{
			_cap_cnt = 0;
			_size    = o._size;
			memcpy(_buf, o._buf, _size);
			return *this;
		}

		/**
		 * Set the destination capability id (server object identity)
		 */
		void destination(Kernel::capid_t id) {
			*reinterpret_cast<long*>(_buf) = id; }

		/**
		 * Return the count of capabilities in the UTCB
		 */
		size_t cap_cnt() { return _cap_cnt; }

		/**
		 * Set the count of capabilities in the UTCB
		 */
		void cap_cnt(size_t cnt) { _cap_cnt = cnt;  }

		/**
		 * Return the start address of the payload data
		 */
		void const * base() const { return &_buf; }

		/**
		 * Copy data from the message buffer 'o' to this UTCB
		 */
		void copy_from(Msgbuf_base &o, size_t size)
		{
			_size = size;

			_cap_cnt = o._snd_cap_cnt;
			for (unsigned i = 0; i < _cap_cnt; i++)
				_caps[i] = o._caps[i].dst();

			memcpy(_buf, o.buf, min(_size, o._size));
		}

		/**
		 * Copy data from this UTCB to the message buffer 'o'
		 */
		void copy_to(Msgbuf_base &o)
		{
			o._snd_cap_cnt = _cap_cnt;
			for (unsigned i = 0; i < _cap_cnt; i++) {
				o._caps[i] = _caps[i];
				if (o._caps[i].valid()) Kernel::ack_cap(o._caps[i].dst());
			}

			memcpy(o.buf, _buf, min(_size, o._size));
		}

		/**
		 * Return the capability id at index 'i'
		 */
		Kernel::capid_t cap_get(unsigned i) {
			return (i < _cap_cnt) ? _caps[i] : Kernel::cap_id_invalid(); }

		/**
		 * Set the capability id 'cap_id' at the next index
		 */
		void cap_add(Kernel::capid_t cap_id) {
			if (_cap_cnt < MAX_CAP_ARGS) _caps[_cap_cnt++] = cap_id; }
};


namespace Genode
{
	static constexpr addr_t VIRT_ADDR_SPACE_START = 0x1000;
	static constexpr size_t VIRT_ADDR_SPACE_SIZE  = 0xfffee000;

	/**
	 * The main thread's UTCB, used during bootstrap of the main thread before it
	 * allocates its context area, needs to be outside the virtual memory area
	 * controlled by the RM session, because it is needed before the main
	 * thread can access its RM session.
	 */
	static constexpr Native_utcb * utcb_main_thread() {
		return (Native_utcb *) (VIRT_ADDR_SPACE_START + VIRT_ADDR_SPACE_SIZE); }
}

#endif /* _BASE__NATIVE_TYPES_H_ */
