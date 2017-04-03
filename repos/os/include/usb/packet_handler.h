/*
 * \brief  Packet stream helper
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB__PACKET_HANDLER_
#define _INCLUDE__USB__PACKET_HANDLER_

#include <base/lock.h>
#include <usb_session/connection.h>

namespace Usb { class Packet_handler; }


class Usb::Packet_handler
{
	private:

		Usb::Connection                  &_connection;
		Genode::Entrypoint               &_ep;

		Io_signal_handler<Packet_handler> _rpc_ack_avail {
			_ep, *this, &Packet_handler::_packet_handler };

		Io_signal_handler<Packet_handler> _rpc_ready_submit {
			_ep, *this, &Packet_handler::_ready_handler };

		bool _ready_submit = true;

		void _packet_handler()
		{
			if (!_ready_submit)
				return;

			while (packet_avail()) {
				Packet_descriptor p = _connection.source()->get_acked_packet();

				if (p.completion)
					p.completion->complete(p);
				else
					release(p);
			}
		}

		void _ready_handler()
		{
			_ready_submit = true;
		};

	public:

		Packet_handler(Connection &connection, Genode::Entrypoint &ep)
		: _connection(connection), _ep(ep)
		{
			/* connect 'ack_avail' to our rpc member */
			_connection.tx_channel()->sigh_ack_avail(_rpc_ack_avail);
			_connection.tx_channel()->sigh_ready_to_submit(_rpc_ready_submit);
		}


		/***************************
		 ** Packet stream wrapper **
		 ***************************/

		bool packet_avail() const
		{
			return _connection.source()->ack_avail();
		}

		void wait_for_packet()
		{
			packet_avail() ? _packet_handler() : _ep.wait_and_dispatch_one_io_signal();
		}

		Packet_descriptor alloc(size_t size)
		{
			/* is size larger than packet stream */
			if (size > _connection.source()->bulk_buffer_size()) {
				Genode::error("packet allocation of (", size, " bytes) too large, "
				              "buffer has ", _connection.source()->bulk_buffer_size(),
				              " bytes");
				throw Usb::Session::Tx::Source::Packet_alloc_failed();
			}

			while (true) {
				try {
					Packet_descriptor p = _connection.source()->alloc_packet(size);
					return p;
				} catch (Usb::Session::Tx::Source::Packet_alloc_failed) {
					/* block until some packets are freed */
					wait_for_packet();
				}
			}
		}

		void submit(Packet_descriptor &p)
		{
			/* check if submit queue is full */
			if (!_connection.source()->ready_to_submit()) {
				_ready_submit = false;

				/* wait for ready_to_submit signal */
				while (!_ready_submit)
					_ep.wait_and_dispatch_one_io_signal();
			}

			_connection.source()->submit_packet(p);

			/*
			 * If an acknowledgement available signal occurred in the meantime,
			 * retrieve packets.
			 */
			if (packet_avail())
				_packet_handler();
		}

		void *content(Packet_descriptor &p)
		{
			return _connection.source()->packet_content(p);
		}

		void release(Packet_descriptor &p)
		{
			_connection.source()->release_packet(p);
		}
};

#endif /* _INCLUDE__USB__PACKET_HANDLER_ */
