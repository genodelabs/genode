/*
 * \brief  Nic client
 * \author Johannes Schlatow
 * \date   2022-06-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NIC_CLIENT_H_
#define _NIC_CLIENT_H_

/* local includes */
#include <interface.h>

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <base/attached_rom_dataspace.h>

namespace Nic_perf {
	class Nic_client;

	using namespace Genode;
}


class Nic_perf::Nic_client
{
	private:
		enum { BUF_SIZE = Nic::Session::QUEUE_SIZE * Nic::Packet_allocator::DEFAULT_PACKET_SIZE };

		Env                       &_env;
		Nic::Packet_allocator      _pkt_alloc;
		Nic::Connection            _nic { _env, &_pkt_alloc, BUF_SIZE, BUF_SIZE };
		Interface                  _interface;

		Signal_handler<Nic_client> _packet_stream_handler
			{ _env.ep(), *this, &Nic_client::_handle_packet_stream };

		void _handle_packet_stream() {
			_interface.handle_packet_stream(); }

	public:

		Nic_client(Env                 &env,
		           Genode::Allocator   &alloc,
		           Xml_node      const &policy,
		           Interface_registry  &registry,
		           Timer::Connection   &timer)
		:
			_env(env),
			_pkt_alloc(&alloc),
			_interface(registry, "nic-client", policy, false, Mac_address(),
			           *_nic.tx(), *_nic.rx(), timer)
		{
			_nic.rx_channel()->sigh_ready_to_ack(_packet_stream_handler);
			_nic.rx_channel()->sigh_packet_avail(_packet_stream_handler);
			_nic.tx_channel()->sigh_ack_avail(_packet_stream_handler);
			_nic.tx_channel()->sigh_ready_to_submit(_packet_stream_handler);

			_interface.handle_packet_stream();
		}
};


#endif /* _NIC_CLIENT_H_ */
