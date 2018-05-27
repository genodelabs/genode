/*
 * \brief  NIC driver based on iPXE
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2011-11-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode */
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <nic/component.h>
#include <nic/root.h>

#include <dde_ipxe/support.h>
#include <dde_ipxe/nic.h>

using namespace Genode;

class Ipxe_session_component  : public Nic::Session_component
{
	public:

		static Ipxe_session_component *instance;

	private:

		Nic::Mac_address _mac_addr;

		static void _rx_callback(unsigned    if_index,
		                         const char *packet,
		                         unsigned    packet_len)
		{
			if (instance)
				instance->_receive(packet, packet_len);
		}

		static void _link_callback()
		{
			if (instance)
				instance->_link_state_changed();
		}

		bool _send()
		{
			using namespace Genode;

			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.size()) {
				Genode::warning("Invalid tx packet");
				return true;
			}

			if (link_state()) {
				if (dde_ipxe_nic_tx(1, _tx.sink()->packet_content(packet), packet.size()))
					Genode::warning("Sending packet failed!");
			}

			_tx.sink()->acknowledge_packet(packet);
			return true;
		}

		void _receive(const char *packet, unsigned packet_len)
		{
			_handle_packet_stream();

			if (!_rx.source()->ready_to_submit())
				return;

			try {
				Nic::Packet_descriptor p = _rx.source()->alloc_packet(packet_len);
				Genode::memcpy(_rx.source()->packet_content(p), packet, packet_len);
				_rx.source()->submit_packet(p);
			} catch (...) {
				Genode::warning(__func__, ": failed to process received packet");
			}
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			while (_send()) ;
		}

	public:

		Ipxe_session_component(Genode::size_t const tx_buf_size,
		                       Genode::size_t const rx_buf_size,
		                       Genode::Allocator   &rx_block_md_alloc,
		                       Genode::Env         &env)
		: Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, env)
		{
			instance = this;

			dde_ipxe_nic_register_callbacks(_rx_callback, _link_callback);

			dde_ipxe_nic_get_mac_addr(1, _mac_addr.addr);

			Genode::log("MAC address ", _mac_addr);
		}

		~Ipxe_session_component()
		{
			instance = nullptr;
			dde_ipxe_nic_unregister_callbacks();
		}

		/**************************************
		 ** Nic::Session_component interface **
		 **************************************/

		Nic::Mac_address mac_address() override { return _mac_addr; }

		bool link_state() override
		{
			return dde_ipxe_nic_link_state(1);
		}
};


Ipxe_session_component *Ipxe_session_component::instance;


struct Main
{
	Env  &_env;

	Heap  _heap { _env.ram(), _env.rm() };

	Nic::Root<Ipxe_session_component> root {_env, _heap };

	Main(Env &env) : _env(env)
	{
		Genode::log("--- iPXE NIC driver started ---");

		Genode::log("-- init iPXE NIC");

		/* pass Env to backend */
		dde_support_init(_env, _heap);

		if (!dde_ipxe_nic_init()) {
			Genode::error("could not find usable NIC device");
		}

		_env.parent().announce(_env.ep().manage(root));
	}
};

void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main main(env);
}
