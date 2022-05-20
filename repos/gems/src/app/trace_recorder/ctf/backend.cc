/*
 * \brief  CTF backend
 * \author Johannes Schlatow
 * \date   2022-05-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <ctf/backend.h>

using namespace Ctf;

void Writer::start_iteration(Directory             &root,
                             Directory::Path const &path,
                             ::Subject_info  const &info)
{
	_file_path = Directory::join(path, info.thread_name());

	try {
		_dst_file.construct(root, _file_path, true);

		/* initialise packet header */
		_packet_buffer.init_header(info);
	}
	catch (New_file::Create_failed)  {
		error("Could not create file."); }
}

void Writer::process_event(Trace_recorder::Trace_event_base const &trace_event, size_t length)
{
	if (!_dst_file.constructed()) return;

	if (trace_event.type() != Trace_recorder::Event_type::CTF) return;

	try {
		/* write to file if buffer is full */
		if (_packet_buffer.bytes_remaining() < length)
			_packet_buffer.write_to_file(*_dst_file, _file_path);

		_packet_buffer.add_event(trace_event.event<Trace_recorder::Ctf_event>(), length - sizeof(Trace_event_base));

	}
	catch (Buffer::Buffer_too_small) {
		error("Packet buffer overflow. (Trace buffer wrapped during read?)"); }
}

void Writer::end_iteration()
{
	/* write buffer to file */
	_packet_buffer.write_to_file(*_dst_file, _file_path);

	_dst_file.destruct();
}
