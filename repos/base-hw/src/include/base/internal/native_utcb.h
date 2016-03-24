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
#include <base/stdint.h>
#include <base/ipc_msgbuf.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* kernel includes */
#include <kernel/interface.h>

namespace Genode { struct Native_utcb; }

class Genode::Native_utcb
{
	public:

		enum { MAX_CAP_ARGS = Msgbuf_base::MAX_CAP_ARGS};

		enum Offsets { THREAD_MYSELF, PARENT, UTCB_DATASPACE };

	private:

		Kernel::capid_t _caps[MAX_CAP_ARGS]; /* capability buffer  */
		size_t          _cap_cnt;            /* capability counter */
		size_t          _size;               /* bytes to transfer  */
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

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_ */
