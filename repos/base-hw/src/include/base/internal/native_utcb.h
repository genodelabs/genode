/*
 * \brief  UTCB definition
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_

/* Genode includes */
#include <util/misc_math.h>
#include <util/string.h>
#include <base/stdint.h>
#include <base/ipc_msgbuf.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* kernel includes */
#include <kernel/interface.h>

namespace Genode {
	
	struct Native_utcb;

	/**
	 * The main thread's UTCB, used during bootstrap of the main thread before it
	 * allocates its stack area, needs to be outside the virtual memory area
	 * controlled by the RM session, because it is needed before the main
	 * thread can access its RM session.
	 * We set it architectural independent to the start of the address space,
	 * but leave out page zero for * null-pointer dereference detection.
	 */
	static constexpr addr_t user_utcb_main_thread() { return get_page_size(); }

	/**
	 * Core and user-land components have different main thread's UTCB locations.
	 */
	Native_utcb * utcb_main_thread();
}


class Genode::Native_utcb
{
	public:

		enum { MAX_CAP_ARGS = Msgbuf_base::MAX_CAPS_PER_MSG};

		enum Offsets { THREAD_MYSELF, PARENT, UTCB_DATASPACE };

	private:

		/*
		 * Note that the members must not be touched at construction time. For
		 * this reason, they are backed by the uninitialized '_raw' array and
		 * indirectly accessed via a 'Header *' pointer.
		 *
		 * The array part beyond the 'Header' is used to carry IPC message
		 * payload.
		 */
		struct Header
		{
			size_t          cap_cnt;            /* capability counter */
			size_t          data_size;          /* bytes to transfer  */
			long            exception_code;     /* result code of RPC */
			Kernel::capid_t destination;        /* invoked object     */
			Kernel::capid_t caps[MAX_CAP_ARGS]; /* capability buffer  */
		};

		uint8_t _raw[get_page_size()];

		Header       &_header()       { return *reinterpret_cast<Header *>      (this); }
		Header const &_header() const { return *reinterpret_cast<Header const *>(this); }

		uint8_t       *_data()       { return &_raw[sizeof(Header)]; }
		uint8_t const *_data() const { return &_raw[sizeof(Header)]; }

		static constexpr size_t _max_data_size = get_page_size() - sizeof(Header);

	public:

		Native_utcb() { }

		Native_utcb& operator= (const Native_utcb &other)
		{
			_header().cap_cnt        = 0;
			_header().data_size      = min(_max_data_size, other._header().data_size);
			_header().exception_code = other._header().exception_code;
			_header().destination    = other._header().destination;
			memcpy(_data(), other._data(), _header().data_size);

			return *this;
		}

		/**
		 * Set the destination capability id (server object identity)
		 */
		void destination(Kernel::capid_t id) { _header().destination = id; }

		/**
		 * Return identity of invoked server object
		 */
		Kernel::capid_t destination() const { return _header().destination; }

		void exception_code(long code) { _header().exception_code = code; }

		long exception_code() const { return _header().exception_code; }

		/**
		 * Return the count of capabilities in the UTCB
		 */
		size_t cap_cnt() const { return _header().cap_cnt; }

		/**
		 * Set the count of capabilities in the UTCB
		 */
		void cap_cnt(size_t cnt) { _header().cap_cnt = cnt;  }

		/**
		 * Return the start address of the payload data
		 */
		void const *data() const { return &_data()[0]; }
		void       *data()       { return &_data()[0]; }

		/**
		 * Return maximum number of bytes for message payload
		 */
		size_t capacity() const { return _max_data_size; }

		/**
		 * Return size of message data in bytes
		 */
		size_t data_size() const { return _header().data_size; }

		/**
		 * Define size of message data to be transferred, in bytes
		 */
		void data_size(size_t data_size)
		{
			_header().data_size = min(data_size, _max_data_size);
		}

		/**
		 * Return the capability id at index 'i'
		 */
		Kernel::capid_t cap_get(unsigned i) const
		{
			return (i < MAX_CAP_ARGS) ? _header().caps[i] : Kernel::cap_id_invalid();
		}

		/**
		 * Set the capability id at index 'i'
		 */
		void cap_set(unsigned i, Kernel::capid_t cap)
		{
			if (i < MAX_CAP_ARGS)
				_header().caps[i] = cap;
		}

		/**
		 * Set the capability id 'cap_id' at the next index
		 */
		void cap_add(Kernel::capid_t cap_id) {
			if (_header().cap_cnt < MAX_CAP_ARGS)
				_header().caps[_header().cap_cnt++] = cap_id; }
};

static_assert(sizeof(Genode::Native_utcb) == Genode::get_page_size(),
              "Native_utcb is not page-sized");

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_ */
