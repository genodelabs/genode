/*
 * \brief  Terminal session component
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/env.h>
#include <base/rpc_server.h>
#include <util/misc_math.h>

/* local includes */
#include "terminal_session_component.h"

using namespace Genode;

Terminal_crosslink::Session_component::Session_component(Env &env,
                                                         Session_component &partner,
                                                         size_t buffer_size)
: _env(env),
  _partner(partner),
  _buffer_size(buffer_size),
  _session_cap(_env.ep().rpc_ep().manage(this)),
  _io_buffer(env.ram(), env.rm(), IO_BUFFER_SIZE),
  _cross_num_bytes_avail(0)
{
}


Session_capability Terminal_crosslink::Session_component::cap()
{
	return _session_cap;
}


bool Terminal_crosslink::Session_component::belongs_to(Session_capability cap)
{
	return _env.ep().rpc_ep().apply(cap, [this] (Session_component *session) {
		return session == this; });
}


bool Terminal_crosslink::Session_component::cross_avail()
{
	return (_cross_num_bytes_avail > 0);
}


size_t Terminal_crosslink::Session_component::cross_read(unsigned char *buf,
                                                         size_t dst_len)
{
	size_t num_bytes_read;

	for (num_bytes_read = 0;
	     (num_bytes_read < dst_len) && !_buffer.empty();
	     num_bytes_read++)
		buf[num_bytes_read] = _buffer.get();

	_cross_num_bytes_avail -= num_bytes_read;

	return num_bytes_read;
}

void Terminal_crosslink::Session_component::cross_write()
{
	Signal_transmitter(_read_avail_sigh).submit();
}


Terminal::Session::Size Terminal_crosslink::Session_component::size()
{ return Terminal::Session::Size(0, 0); }


bool Terminal_crosslink::Session_component::avail()
{
	return _partner.cross_avail();
}


size_t Terminal_crosslink::Session_component::_read(size_t dst_len)
{
	return _partner.cross_read(_io_buffer.local_addr<unsigned char>(),
	                           min(dst_len, _io_buffer.size()));
}


size_t Terminal_crosslink::Session_component::_write(size_t num_bytes)
{
	num_bytes = min(num_bytes, _io_buffer.size());

	unsigned char *src = _io_buffer.local_addr<unsigned char>();

	size_t num_bytes_written = 0;
	bool error = false;

	while ((num_bytes_written < num_bytes) && !error)
		_buffer.add(src[num_bytes_written]).with_result(
			[&] (Ring_buffer::Add_ok)    { ++num_bytes_written; },
			[&] (Ring_buffer::Add_error) { error = true; }
		);

	_cross_num_bytes_avail += num_bytes_written;
	_partner.cross_write();

	return num_bytes_written;
}


Dataspace_capability Terminal_crosslink::Session_component::_dataspace()
{ return _io_buffer.cap(); }


void Terminal_crosslink::Session_component::connected_sigh(Signal_context_capability sigh)
{
	/*
	 * Immediately reflect connection-established signal to the
	 * client because the session is ready to use immediately after
	 * creation.
	 */
	Genode::Signal_transmitter(sigh).submit();
}


void Terminal_crosslink::Session_component::read_avail_sigh(Signal_context_capability sigh)
{
	_read_avail_sigh = sigh;
}


size_t Terminal_crosslink::Session_component::read(void *, size_t)
{ return 0; }


size_t Terminal_crosslink::Session_component::write(void const *, size_t)
{ return 0; }
