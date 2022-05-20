/*
 * \brief  Packet header and context types
 * \author Johannes Schlatow
 * \date   2021-08-04
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__PACKET_TYPES_H_
#define _CTF__PACKET_TYPES_H_

#include <base/trace/types.h>
#include <ctf/timestamp.h>
#include <util/register.h>

namespace Ctf {
	using namespace Genode;
	using Genode::Trace::Thread_name;
	struct Packet_header;
}

/**
 * Definition of our CTF packet header. A CTF stream may contain multiple
 * packets that bundle an arbitrary number of events. In order to reduce the
 * payload for every CTF event, we put shared information (such as session and
 * thread name) into the packet header.
 *
 * See https://diamon.org/ctf/ for the CTF spec.
 */
struct Ctf::Packet_header
{
	uint32_t       _magic                  { 0xC1FC1FC1 };
	uint32_t       _stream_id              { };
	Timestamp_base _timestamp_start        { };
	Timestamp_base _timestamp_end          { };
	uint32_t       _total_length           { };
	uint16_t       _hdr_length             { sizeof(Packet_header) * 8 };
	uint16_t       _affinity               { };
	uint8_t        _priority               { };
	char           _session_and_thread[0]  { };

	struct Affinity : Register<16>
	{
		struct Xpos   : Bitfield<0,4>  { };
		struct Ypos   : Bitfield<4,4>  { };
		struct Width  : Bitfield<8,4>  { };
		struct Height : Bitfield<12,4> { };
	};

	Packet_header(Session_label              const &label,
	              Thread_name                const &thread,
	              Genode::Affinity::Location const &affinity,
	              unsigned                          priority,
	              Genode::size_t                    buflen,
	              unsigned                          streamid=0)
	: _stream_id(streamid),
	  _affinity(Affinity::Xpos::bits((uint16_t)affinity.xpos())   |
	            Affinity::Ypos::bits((uint16_t)affinity.ypos())   |
	            Affinity::Width::bits((uint16_t)affinity.width()) |
	            Affinity::Height::bits((uint16_t)affinity.height())),
	  _priority((uint8_t)priority)
	{
		Genode::size_t char_offset = 0;

		if (_hdr_length/8 < buflen) {
			Genode::size_t sess_len = Genode::min(label.length(), buflen - _hdr_length/8);
			Genode::copy_cstring(&_session_and_thread[char_offset], label.string(), sess_len);
			_hdr_length += (uint16_t)(sess_len * 8);
			char_offset = sess_len;
		}

		if (_hdr_length/8 < buflen) {
			Genode::size_t thread_len = Genode::min(thread.length(), buflen - _hdr_length/8);
			Genode::copy_cstring(&_session_and_thread[char_offset], thread.string(), thread_len);
			_hdr_length += (uint16_t)(thread_len * 8);
		}

		_total_length = _hdr_length;
	}

	void reset()
	{
		_total_length    = _hdr_length;
		_timestamp_start = 0;
		_timestamp_end   = 0;
	}

	/**
	 * If event fits into provided buffer, update struct with timestamp and
	 * length of new event. Makes sure that timestamps are monotonically
	 * increasing and calls the provided functor(char * ptr, Timestamp_base ts),
	 * where ptr is the target address and ts is the updated timestamp.
	 */
	template <typename FUNC>
	void append_event(char          * buffer,
	                  Genode::size_t  bufsz,
	                  Timestamp_base  timestamp,
	                  Genode::size_t  length,
	                  FUNC         && fn)
	{
		using Ctf::Timestamp;

		/* check whether event fits into buffer */
		if (total_length_bytes()+length > bufsz)
			return;

		/* update timestamps */
		if (_timestamp_start == 0)
			_timestamp_start = timestamp;
		else if (Timestamp::extended() &&
		         Timestamp::Base::get(_timestamp_end) > Timestamp::Base::get(timestamp)) {
			/* timer wrapped, increase manually managed offset */
			Timestamp::Extension::set(_timestamp_end, Timestamp::Extension::get(_timestamp_end)+1);
		}
		Timestamp::Base::set(_timestamp_end, timestamp);

		/* call provided functor with target address and modified timestamp */
		fn(&buffer[total_length_bytes()], _timestamp_end);

		_total_length += (uint32_t)(length * 8);
	}

	uint32_t total_length_bytes() const { return _total_length / 8; }
	bool     empty()              const { return _total_length <= _hdr_length; }

} __attribute__((packed));

#endif /* _CTF__PACKET_TYPES_H_ */
