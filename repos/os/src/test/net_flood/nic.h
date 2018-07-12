/*
 * \brief  NIC connection wrapper for a more convenient interface
 * \author Martin Stein
 * \date   2018-04-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NIC_H_
#define _NIC_H_

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <net/ethernet.h>

namespace Genode {

	class Env;
}

namespace Net {

	struct Nic_handler;
	class  Nic;

	using Packet_descriptor    = ::Nic::Packet_descriptor;
	using Packet_stream_sink   = ::Nic::Packet_stream_sink< ::Nic::Session::Policy>;
	using Packet_stream_source = ::Nic::Packet_stream_source< ::Nic::Session::Policy>;
}


struct Net::Nic_handler
{
	virtual void handle_eth(Ethernet_frame &eth,
	                        Size_guard     &size_guard) = 0;

	virtual ~Nic_handler() { }
};


class Net::Nic
{
	private:

		using Signal_handler = Genode::Signal_handler<Nic>;

		enum { PKT_SIZE = ::Nic::Packet_allocator::DEFAULT_PACKET_SIZE };
		enum { BUF_SIZE = 1000 * PKT_SIZE };

		Genode::Env            &_env;
		Genode::Allocator      &_alloc;
		Nic_handler            &_handler;
		bool             const &_verbose;
		::Nic::Packet_allocator _pkt_alloc     { &_alloc };
		::Nic::Connection       _nic           { _env, &_pkt_alloc, BUF_SIZE, BUF_SIZE };
		Signal_handler          _sink_ack      { _env.ep(), *this, &Nic::_ack_avail };
		Signal_handler          _sink_submit   { _env.ep(), *this, &Nic::_ready_to_submit };
		Signal_handler          _source_ack    { _env.ep(), *this, &Nic::_ready_to_ack };
		Signal_handler          _source_submit { _env.ep(), *this, &Nic::_packet_avail };
		Mac_address      const  _mac           { _nic.mac_address() };

		Net::Packet_stream_sink   &_sink()   { return *_nic.rx(); }
		Net::Packet_stream_source &_source() { return *_nic.tx(); }


		/***********************************
		 ** Packet-stream signal handlers **
		 ***********************************/

		void _ready_to_submit();
		void _ack_avail() { }
		void _ready_to_ack();
		void _packet_avail() { }

	public:

		Nic(Genode::Env       &env,
		    Genode::Allocator &alloc,
		    Nic_handler       &handler,
		    bool        const &verbose)
		:
			_env     (env),
			_alloc   (alloc),
			_handler (handler),
			_verbose (verbose)
		{
			/* install packet stream signals */
			_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
			_nic.rx_channel()->sigh_packet_avail(_sink_submit);
			_nic.tx_channel()->sigh_ack_avail(_source_ack);
			_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
		}

		template <typename FUNC>
		void send(Genode::size_t pkt_size,
		          FUNC        && write_to_pkt)
		{
			Packet_descriptor  pkt      = _source().alloc_packet(pkt_size);
			void              *pkt_base = _source().packet_content(pkt);
			Size_guard size_guard(pkt_size);
			write_to_pkt(pkt_base, size_guard);
			_source().submit_packet(pkt);
			if (_verbose) {
				Size_guard size_guard(pkt_size);
				try { Genode::log("snd ", Ethernet_frame::cast_from(pkt_base, size_guard)); }
				catch (Size_guard::Exceeded) { Genode::log("snd ?"); }
			}
		}


		/***************
		 ** Accessors **
		 ***************/

		Mac_address const &mac() const { return _mac; }
};


#endif /* _NIC_H_ */
