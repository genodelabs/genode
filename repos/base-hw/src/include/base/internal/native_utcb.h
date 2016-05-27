/*
 * \brief  UTCB definition
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

	static constexpr addr_t VIRT_ADDR_SPACE_START = 0x1000;
	static constexpr size_t VIRT_ADDR_SPACE_SIZE  = 0xfffee000;

	/**
	 * The main thread's UTCB, used during bootstrap of the main thread before it
	 * allocates its stack area, needs to be outside the virtual memory area
	 * controlled by the RM session, because it is needed before the main
	 * thread can access its RM session.
	 */
	static constexpr Native_utcb * utcb_main_thread() {
		return (Native_utcb *) (VIRT_ADDR_SPACE_START + VIRT_ADDR_SPACE_SIZE); }
}


class Genode::Native_utcb
{
	public:

		enum { MAX_CAP_ARGS = Msgbuf_base::MAX_CAPS_PER_MSG};

		enum Offsets { THREAD_MYSELF, PARENT, UTCB_DATASPACE };

	private:

		/*
		 * Note thats the member variables are sorted from the largest to the
		 * smallest size to avoid padding. Otherwise, the dimensioning of
		 * '_data' would not yield a page-sized 'Native_utcb'.
		 */

		size_t          _cap_cnt;            /* capability counter */
		size_t          _data_size;          /* bytes to transfer  */
		long            _exception_code;     /* result code of RPC */
		Kernel::capid_t _destination;        /* invoked object     */
		Kernel::capid_t _caps[MAX_CAP_ARGS]; /* capability buffer  */
		uint8_t         _data[get_page_size() - sizeof(_caps) -
		                      sizeof(_cap_cnt) - sizeof(_data_size) -
		                      sizeof(_destination) - sizeof(_exception_code)];

	public:

		Native_utcb& operator= (const Native_utcb &other)
		{
			_cap_cnt        = 0;
			_data_size      = min(sizeof(_data), other._data_size);
			_exception_code = other._exception_code;
			_destination    = other._destination;
			memcpy(_data, other._data, _data_size);

			return *this;
		}

		/**
		 * Set the destination capability id (server object identity)
		 */
		void destination(Kernel::capid_t id) { _destination = id; }

		/**
		 * Return identity of invoked server object
		 */
		Kernel::capid_t destination() const { return _destination; }

		void exception_code(long code) { _exception_code = code; }

		long exception_code() const { return _exception_code; }

		/**
		 * Return the count of capabilities in the UTCB
		 */
		size_t cap_cnt() const { return _cap_cnt; }

		/**
		 * Set the count of capabilities in the UTCB
		 */
		void cap_cnt(size_t cnt) { _cap_cnt = cnt;  }

		/**
		 * Return the start address of the payload data
		 */
		void const *data() const { return &_data[0]; }
		void       *data()       { return &_data[0]; }

		/**
		 * Return maximum number of bytes for message payload
		 */
		size_t capacity() const { return sizeof(_data); }

		/**
		 * Return size of message data in bytes
		 */
		size_t data_size() const { return _data_size; }

		/**
		 * Define size of message data to be transferred, in bytes
		 */
		void data_size(size_t data_size)
		{
			_data_size = min(data_size, sizeof(_data));
		}

		/**
		 * Return the capability id at index 'i'
		 */
		Kernel::capid_t cap_get(unsigned i) const
		{
			return (i < MAX_CAP_ARGS) ? _caps[i] : Kernel::cap_id_invalid();
		}

		/**
		 * Set the capability id at index 'i'
		 */
		void cap_set(unsigned i, Kernel::capid_t cap)
		{
			if (i < MAX_CAP_ARGS)
				_caps[i] = cap;
		}

		/**
		 * Set the capability id 'cap_id' at the next index
		 */
		void cap_add(Kernel::capid_t cap_id) {
			if (_cap_cnt < MAX_CAP_ARGS) _caps[_cap_cnt++] = cap_id; }
};

static_assert(sizeof(Genode::Native_utcb) == Genode::get_page_size(),
              "Native_utcb is not page-sized");

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_ */
