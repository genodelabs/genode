/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2017-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

/* local includes */
#include <pointer.h>

/* Genode includes */
#include <nic_session/nic_session.h>
#include <util/string.h>
#include <timer_session/connection.h>

namespace Net {

	using Packet_descriptor    = ::Nic::Packet_descriptor;
	using Packet_stream_sink   = ::Nic::Packet_stream_sink< ::Nic::Session::Policy>;
	using Packet_stream_source = ::Nic::Packet_stream_source< ::Nic::Session::Policy>;
	class Ethernet_frame;
	class Interface;
	using Interface_label = Genode::String<64>;
}


class Net::Interface
{
	protected:

		using Signal_handler = Genode::Signal_handler<Interface>;

		Signal_handler    _sink_ack;
		Signal_handler    _sink_submit;
		Signal_handler    _source_ack;
		Signal_handler    _source_submit;

	private:

		Genode::Allocator  &_alloc;
		Pointer<Interface>  _remote;
		Interface_label     _label;
		Timer::Connection  &_timer;
		unsigned           &_curr_time;
		bool                _log_time;

		void _send(Ethernet_frame &eth, Genode::size_t const eth_size);

		void _handle_eth(void              *const  eth_base,
		                 Genode::size_t     const  eth_size,
		                 Packet_descriptor  const &pkt);

		virtual Packet_stream_sink &_sink() = 0;

		virtual Packet_stream_source &_source() = 0;


		/***********************************
		 ** Packet-stream signal handlers **
		 ***********************************/

		void _ready_to_submit();
		void _ack_avail() { }
		void _ready_to_ack();
		void _packet_avail() { }

	public:

		Interface(Genode::Entrypoint &ep,
		          Interface_label     label,
		          Timer::Connection  &timer,
		          unsigned           &curr_time,
		          bool                log_time,
		          Genode::Allocator  &alloc);

		void remote(Interface &remote) { _remote.set(remote); }
};

#endif /* _INTERFACE_H_ */
