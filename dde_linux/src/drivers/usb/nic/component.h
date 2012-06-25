/*
 * \brief  Block-session implementation for network devices
 * \author Sebastian Sumpf
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NIC__COMPONENT_H_
#define _NIC__COMPONENT_H_

#include <root/component.h>
#include <nic_session/rpc_object.h>

#include <signal/dispatch.h>

namespace Nic {

	using namespace Genode;


	class Session_component;

	struct Device : ::Device
	{
		Session_component *_session;

		/**
		 * Transmit data to driver
		 */
		virtual void tx(addr_t virt, size_t size) = 0;

		/**
		 * Return mac address of device
		 */
		virtual Mac_address mac_address() = 0;

		/**
		 * Set session belonging to this driver
		 */
		void session(Session_component *s) { _session = s; }
	};

	class Session_component : public Genode::Allocator_avl,
	                          public Packet_session_component<Session_rpc_object>
	{
		private:

			Device   *_device;   /* device this session is using */
			Tx::Sink *_tx_sink;  /* client packet sink */

		protected:

			void _process_packets()
			{
				/* submit received packets to lower layer */
				while (_tx_sink->packet_avail())
				{
					Packet_descriptor packet = _tx_sink->get_packet();
					addr_t virt = (addr_t)_tx_sink->packet_content(packet);
					/* send to driver */
					_device->tx(virt, packet.size());

					if (!_tx_sink->ready_to_ack())
						PWRN("Wait for TX packet ack");

					/* acknowledge to client */
					_tx_sink->acknowledge_packet(packet);
				}

				/* release acknowledged packets */
				while (_rx.source()->ack_avail())
				{
					
					Packet_descriptor packet = _rx.source()->get_acked_packet();

					/* free packet buffer */
					_rx.source()->release_packet(packet);
				}
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(Dataspace_capability  tx_ds,
			                  Dataspace_capability  rx_ds,
			                  Rpc_entrypoint       &ep,
			                  Signal_receiver *sig_rec,
			                  ::Device *device)
			:
				Genode::Allocator_avl(Genode::env()->heap()),
				Packet_session_component(tx_ds, rx_ds, this,  ep, sig_rec),
				_device(static_cast<Device *>(device)),
				_tx_sink(Session_rpc_object::_tx.sink()) { _device->session(this); }

			Mac_address mac_address() { return _device->mac_address(); }

			/**
			 * Send packet to client (called form driver)
			 */
			void rx(addr_t virt, size_t size)
			{
				Packet_descriptor p =_rx.source()->alloc_packet(size);
				memcpy(_rx.source()->packet_content(p), (void*)virt, size);
				_rx.source()->submit_packet(p);
			}
	};

	/*
	 * Shortcut for single-client root component
	 */
	typedef Root_component<Session_component, Single_client> Root_component;

	/**
	 * Root component, handling new session requests
	 */
	class Root : public Packet_root<Root_component, Session_component>
	{
		public:

			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc,
			     Signal_receiver *sig_rec, Device *device)
			:
				Packet_root(session_ep, md_alloc, sig_rec, device) { }
	};
}


#endif /* _NIC__COMPONENT_H_ */
