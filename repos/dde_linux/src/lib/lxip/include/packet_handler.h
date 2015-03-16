/*
 * \brief  Signal driven network handler
 * \author Stefan Kalkowski
 * \auhtor Sebastian Sumpf
 * \date   2013-09-24
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PACKET_HANDLER_H_
#define _PACKET_HANDLER_H_

/* Genode */
#include <nic_session/connection.h>

namespace Net {

	class Packet_handler;

	using Nic::Packet_stream_sink;
	using Nic::Packet_stream_source;
	using Nic::Packet_descriptor;
}


/**
 * Generic packet handler used as base for NIC and client packet handlers.
 */
class Net::Packet_handler
{
	protected:

		/**
		 * submit queue not empty anymore
		 */
		void _packet_avail(unsigned);

		/**
		 * acknoledgement queue not full anymore
		 */
		void _ready_to_ack(unsigned);

		/**
		 * acknoledgement queue not empty anymore
		 */
		void _ack_avail(unsigned);

		/**
		 * submit queue not full anymore
		 *
		 * TODO: by now, we just drop packets that cannot be transferred
		 *       to the other side, that's why we ignore this signal.
		 */
		void _ready_to_submit(unsigned) { }

		Genode::Signal_dispatcher<Packet_handler> _sink_ack;
		Genode::Signal_dispatcher<Packet_handler> _sink_submit;
		Genode::Signal_dispatcher<Packet_handler> _source_ack;
		Genode::Signal_dispatcher<Packet_handler> _source_submit;

	public:

		Packet_handler();

		virtual Packet_stream_sink< ::Nic::Session::Policy>   * sink()   = 0;
		virtual Packet_stream_source< ::Nic::Session::Policy> * source() = 0;
};

#endif /* _PACKET_HANDLER_H_ */
