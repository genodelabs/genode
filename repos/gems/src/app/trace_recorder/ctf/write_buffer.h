/*
 * \brief  Convenience helper for creating a CTF packet
 * \author Johannes Schlatow
 * \date   2021-08-06
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__WRITE_BUFFER_H_
#define _CTF__WRITE_BUFFER_H_

/* local includes */
#include <subject_info.h>
#include <ctf/packet_header.h>

/* Genode includes */
#include <os/vfs.h>
#include <trace_recorder_policy/ctf.h>

namespace Ctf {
	template <unsigned>
	class Write_buffer;
}

template <unsigned BUFSIZE>
class Ctf::Write_buffer
{
	public:
		struct Buffer_too_small : Genode::Exception { };

	private:
		char _buffer[BUFSIZE] { };

		Packet_header &_header() { return *(Packet_header*)_buffer; }

	public:

		void init_header(::Subject_info const &info)
		{
			construct_at<Packet_header>(_buffer,
			                                 info.session_label(),
			                                 info.thread_name(),
			                                 info.affinity(),
			                                 info.priority(),
			                                 BUFSIZE);
		}

		void add_event(Ctf::Event_header_base const &event, Genode::size_t length)
		{
			_header().append_event(_buffer, BUFSIZE,
			                       event.timestamp(), length,
			                       [&] (char * ptr, Timestamp_base ts)
			{
				/* copy event into buffer */
				memcpy(ptr, &event, length);

				/* update timestamp */
				((Event_header_base *)ptr)->timestamp(ts);
			});
		}

		void write_to_file(Genode::New_file &dst, Genode::Directory::Path const &path)
		{
			if (_header().empty())
				return;

			if (dst.append(_buffer, _header().total_length_bytes()) != New_file::Append_result::OK)
				error("Write error for ", path);

			_header().reset();
		}

		Genode::size_t bytes_remaining() { return BUFSIZE - _header().total_length_bytes(); }
};


#endif /* _CTF__WRITE_BUFFER_H_ */
