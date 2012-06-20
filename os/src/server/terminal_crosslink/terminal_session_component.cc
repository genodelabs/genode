/*
 * \brief  Terminal session component
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <base/signal.h>
#include <util/misc_math.h>

/* local includes */
#include "terminal_session_component.h"

using namespace Genode;

namespace Terminal {

Session_component::Session_component(Session_component &partner, Cap_session &cap_session, const char *ep_name)
: _partner(partner),
  _ep(&cap_session, STACK_SIZE, ep_name),
  _session_cap(_ep.manage(this)),
  _io_buffer(Genode::env()->ram_session(), BUFFER_SIZE),
  _cross_num_bytes_avail(0)
{
}


Genode::Session_capability Session_component::cap()
{
	return _session_cap;
}


bool Session_component::belongs_to(Genode::Session_capability cap)
{
        return _ep.obj_by_cap(cap) == this;
}


bool Session_component::cross_avail()
{
	return (_cross_num_bytes_avail > 0);
}


size_t Session_component::cross_read(unsigned char *buf, size_t dst_len)
{
	size_t num_bytes_read;

	for (num_bytes_read = 0;
	     (num_bytes_read < dst_len) && !_buffer.empty();
	     num_bytes_read++)
		buf[num_bytes_read] = _buffer.get();

	_cross_num_bytes_avail -= num_bytes_read;

	_write_avail_lock.unlock();

	return num_bytes_read;
}

void Session_component::cross_write()
{
	Signal_transmitter(_read_avail_sigh).submit();
}


Session::Size Session_component::size() { return Size(0, 0); }


bool Session_component::avail()
{
	return _partner.cross_avail();
}


Genode::size_t Session_component::_read(Genode::size_t dst_len)
{
	return _partner.cross_read(_io_buffer.local_addr<unsigned char>(), dst_len);
}


void Session_component::_write(Genode::size_t num_bytes)
{
	unsigned char *src = _io_buffer.local_addr<unsigned char>();

	size_t num_bytes_written = 0;
	size_t src_index = 0;
	while (num_bytes_written < num_bytes)
		try {
			_buffer.add(src[src_index]);
			++src_index;
			++num_bytes_written;
		} catch(Local_buffer::Overflow) {
			_cross_num_bytes_avail += num_bytes_written;

			/* Lock the lock (always succeeds) */
			_write_avail_lock.lock();

			_partner.cross_write();

			/*
			 * This lock operation blocks or not, depending on whether the
			 * partner already has called 'cross_read()' in the meantime
			 */
			_write_avail_lock.lock();

			/*
			 * Unlock the lock, so it is unlocked the next time the exception
			 * triggers
			 */
			_write_avail_lock.unlock();

			num_bytes -= num_bytes_written;
			num_bytes_written = 0;
		}

	_cross_num_bytes_avail += num_bytes_written;
	_partner.cross_write();
}


Genode::Dataspace_capability Session_component::_dataspace() { return _io_buffer.cap(); }


void Session_component::connected_sigh(Genode::Signal_context_capability sigh)
{
	/*
	 * Immediately reflect connection-established signal to the
	 * client because the session is ready to use immediately after
	 * creation.
	 */
	Genode::Signal_transmitter(sigh).submit();
}


void Session_component::read_avail_sigh(Genode::Signal_context_capability sigh)
{
	_read_avail_sigh = sigh;
}


Genode::size_t Session_component::read(void *, Genode::size_t) { return 0; }


Genode::size_t Session_component::write(void const *, Genode::size_t) { return 0; }

}
