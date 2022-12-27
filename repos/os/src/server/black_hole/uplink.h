/*
 * \brief  Uplink session component and root
 * \author Martin Stein
 * \date   2022-02-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_H_
#define _UPLINK_H_

/* base includes */
#include <base/ram_allocator.h>
#include <root/component.h>

/* os includes */
#include <nic/packet_allocator.h>
#include <uplink_session/rpc_object.h>

namespace Black_hole {

	using namespace Genode;

	class Uplink_session_base;
	class Uplink_session;
	class Uplink_root;
}


class Black_hole::Uplink_session_base
{
	friend class Uplink_session;

	private:

		class Buffer
		{
			private:

				Ram_allocator            &_ram_alloc;
				Ram_dataspace_capability  _ram_ds;

			public:

				Buffer(Ram_allocator &ram_alloc,
				       size_t const   size)
				:
					_ram_alloc { ram_alloc },
					_ram_ds    { ram_alloc.alloc(size) }
				{ }

				~Buffer() { _ram_alloc.free(_ram_ds); }

				Dataspace_capability ds() const { return _ram_ds; }
		};

		Env                   &_env;
		Allocator             &_alloc;
		Nic::Packet_allocator  _packet_alloc;
		Buffer                 _tx_buf;
		Buffer                 _rx_buf;

	public:

		Uplink_session_base(Env       &env,
		                    size_t     tx_buf_size,
		                    size_t     rx_buf_size,
		                    Allocator &alloc)
		:
			_env          { env },
			_alloc        { alloc },
			_packet_alloc { &_alloc },
			_tx_buf       { _env.ram(), tx_buf_size },
			_rx_buf       { _env.ram(), rx_buf_size }
		{ }
};


class Black_hole::Uplink_session : private Uplink_session_base,
                                   public  Uplink::Session_rpc_object
{
	private:

		Signal_handler<Uplink_session> _packet_stream_handler;

		void _handle_packet_stream()
		{
			while (_tx.sink()->packet_avail()) {

				if (!_tx.sink()->ready_to_ack()) {
					return;
				}
				Packet_descriptor const pkt { _tx.sink()->get_packet() };
				if (!pkt.size() || !_tx.sink()->packet_valid(pkt)) {
					continue;
				}
				_tx.sink()->acknowledge_packet(pkt);
			}
		}

	public:

		Uplink_session(Env       &env,
		               size_t     tx_buf_size,
		               size_t     rx_buf_size,
		               Allocator &alloc)
		:
			Uplink_session_base { env, tx_buf_size,rx_buf_size, alloc },
			Session_rpc_object {
				_env.rm(), _tx_buf.ds(), _rx_buf.ds() ,&_packet_alloc,
				_env.ep().rpc_ep() },

			_packet_stream_handler {
				env.ep(), *this, &Uplink_session::_handle_packet_stream }
		{
			_tx.sigh_ready_to_ack   (_packet_stream_handler);
			_tx.sigh_packet_avail   (_packet_stream_handler);
			_rx.sigh_ack_avail      (_packet_stream_handler);
			_rx.sigh_ready_to_submit(_packet_stream_handler);
		}
};


class Black_hole::Uplink_root : public Root_component<Uplink_session>
{
	private:

		Env &_env;

		Uplink_session *_create_session(char const *args) override
		{
			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
				      "need ", tx_buf_size + rx_buf_size);
				throw Insufficient_ram_quota();
			}

			return new (md_alloc())
				Uplink_session(_env, tx_buf_size, rx_buf_size, *md_alloc());
		}

	public:

		Uplink_root(Env       &env,
		            Allocator &alloc)
		:
			Root_component<Uplink_session> { &env.ep().rpc_ep(), &alloc },
			_env                           { env }
		{ }
};

#endif /* _UPLINK_H_ */
