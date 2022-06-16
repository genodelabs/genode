/*
 * \brief  Uplink root and session component
 * \author Johannes Schlatow
 * \date   2022-06-17
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_ROOT_H_
#define _UPLINK_ROOT_H_

/* local includes */
#include <interface.h>

/* Genode includes */
#include <root/component.h>
#include <uplink_session/rpc_object.h>
#include <nic/packet_allocator.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>

namespace Nic_perf {
	class Uplink_session_base;
	class Uplink_session_component;
	class Uplink_root;

	using namespace Genode;
}


class Nic_perf::Uplink_session_base
{
	friend class Uplink_session_component;

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


class Nic_perf::Uplink_session_component : private Uplink_session_base,
                                           public  Uplink::Session_rpc_object
{
	private:

		Interface                                _interface;
		Signal_handler<Uplink_session_component> _packet_stream_handler
			{ _env.ep(), *this, &Uplink_session_component::_handle_packet_stream };

		void _handle_packet_stream() {
			_interface.handle_packet_stream(); }

	public:

		Uplink_session_component(size_t        const  tx_buf_size,
		                         size_t        const  rx_buf_size,
		                         Allocator           &alloc,
		                         Env                 &env,
		                         Session_label const &label,
		                         Xml_node      const &policy,
		                         Interface_registry  &registry,
		                         Mac_address          mac,
		                         Timer::Connection   &timer)
		:
			Uplink_session_base(env, tx_buf_size, rx_buf_size, alloc),
			Uplink::Session_rpc_object(env.rm(), _tx_buf.ds(), _rx_buf.ds(),
			                           &_packet_alloc, env.ep().rpc_ep()),
			_interface(registry, label, policy, false, mac,
			           *_rx.source(), *_tx.sink(), timer)
		{
			_interface.handle_packet_stream();

			_tx.sigh_ready_to_ack   (_packet_stream_handler);
			_tx.sigh_packet_avail   (_packet_stream_handler);
			_rx.sigh_ack_avail      (_packet_stream_handler);
			_rx.sigh_ready_to_submit(_packet_stream_handler);
		}
};


class Nic_perf::Uplink_root : public Root_component<Uplink_session_component>
{
	private:
		Env                    &_env;
		Attached_rom_dataspace &_config;
		Interface_registry     &_registry;
		Timer::Connection      &_timer;

	protected:

		Uplink_session_component *_create_session(char const *args) override
		{
			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (size_t)sizeof(Uplink_session_component));
			if (ram_quota < session_size)
				throw Insufficient_ram_quota();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
				      "need ", tx_buf_size + rx_buf_size + session_size);
				throw Insufficient_ram_quota();
			}

			enum { MAC_STR_LENGTH = 19 };
			char mac_str [MAC_STR_LENGTH];
			Arg mac_arg { Arg_string::find_arg(args, "mac_address") };

			if (!mac_arg.valid())
				throw Service_denied();

			mac_arg.string(mac_str, MAC_STR_LENGTH, "");
			Mac_address mac { };
			ascii_to(mac_str, mac);
			if (mac == Mac_address { })
				throw Service_denied();

			Session_label label = label_from_args(args);

			Session_policy policy(label, _config.xml());

			return new (md_alloc()) Uplink_session_component(tx_buf_size, rx_buf_size,
			                                                 *md_alloc(), _env, label, policy,
			                                                 _registry, mac, _timer);
		}

	public:

		Uplink_root(Env                    &env,
		            Allocator              &md_alloc,
		            Interface_registry     &registry,
		            Attached_rom_dataspace &config,
		            Timer::Connection      &timer)
		:
			Root_component<Uplink_session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env),
			_config(config),
			_registry(registry),
			_timer(timer)
		{ }
};

#endif /* _UPLINK_ROOT_H_ */
