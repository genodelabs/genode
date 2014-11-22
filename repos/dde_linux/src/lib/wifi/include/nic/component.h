/*
 * \brief  Nic-session implementation for network devices
 * \author Sebastian Sumpf
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NIC__COMPONENT_H_
#define _NIC__COMPONENT_H_

/* Genode includes */
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <root/component.h>
#include <timer_session/connection.h>

/* local includes */
#include <nic/dispatch.h>

#include <extern_c_begin.h>
# include <linux/skbuff.h>
#include <extern_c_end.h>


namespace Nic {

	using namespace Genode; /* FIXME */

	struct Session_component;
	struct Device;

	typedef Genode::Root_component<Session_component, Single_client> Root_component;

	struct Root;
}


struct Nic::Device
{
	/**
	 * Transmit data to driver
	 */
	virtual bool tx(addr_t virt, size_t size) = 0;

	/**
	 * Return mac address of device
	 */
	virtual Mac_address mac_address() = 0;

	/**
	 * Return link state (true if link detected)
	 */
	virtual bool link_state() = 0;

	/**
	 * Set session belonging to this driver
	 */
	virtual void session(Session_component *s) = 0;

	Device() { }
	virtual ~Device() { }
};


class Nic::Session_component : public Nic::Packet_allocator,
                               public Packet_session_component<Session_rpc_object>
{
	private:

		Device           &_device;    /* device this session is using */
		Tx::Sink         *_tx_sink;   /* client packet sink */
		bool              _tx_alloc;  /* get next packet from client or use _tx_packet */
		Packet_descriptor _tx_packet; /* saved packet in case of driver errors */

		Signal_context_capability _link_state_sigh;

		void _send_packet_avail_signal() {
			Signal_transmitter(_tx.sigh_packet_avail()).submit(); }

	protected:

		void _process_packets(unsigned)
		{
			int tx_cnt         = 0;

			/* submit received packets to lower layer */
			while (_tx_sink->packet_avail()) {
				Packet_descriptor packet = _tx_alloc ? _tx_sink->get_packet() : _tx_packet;
				addr_t virt = (addr_t)_tx_sink->packet_content(packet);

				/* send to driver */
				if (!(_device.tx(virt, packet.size()))) {
					_send_packet_avail_signal();
					_tx_alloc  = false;
					_tx_packet = packet;
					return;
				}

				_tx_alloc = true;
				tx_cnt++;

				/* acknowledge to client */
				_tx_sink->acknowledge_packet(packet);
			}

			/* release acknowledged packets */
			_rx_ack(false);

			if (_tx_sink->packet_avail())
				_send_packet_avail_signal();
		}

		void _rx_ack(bool block = true)
		{
			while (_rx.source()->ack_avail() || block) {
				Packet_descriptor packet = _rx.source()->get_acked_packet();

				/* free packet buffer */
				_rx.source()->release_packet(packet);
				block = false;
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Dataspace_capability  tx_ds,
		                  Dataspace_capability  rx_ds,
		                  Server::Entrypoint   &ep,
		                  Device               &device)
		:
			Nic::Packet_allocator(Genode::env()->heap()),
			Packet_session_component(tx_ds, rx_ds, this, ep),
			_device(device),
			_tx_sink(Session_rpc_object::_tx.sink()),
			_tx_alloc(true)
			{ _device.session(this); }

		/**
		 * Send packet to client (called from driver)
		 */
		void rx(addr_t packet, size_t psize, addr_t frag, size_t fsize)
		{
			size_t size = psize + fsize;

			while (true) {
				try {
					Packet_descriptor p =_rx.source()->alloc_packet(size);

					Genode::memcpy(_rx.source()->packet_content(p), (void*)packet, psize);

					if (fsize)
						Genode::memcpy(_rx.source()->packet_content(p)+psize, (void*)frag, fsize);

					_rx.source()->submit_packet(p);
				} catch (...) {
					/* ack or block */
					_rx_ack();
					continue;
				}
				break;
			}

			_rx_ack(false);
		}

		/**
		 * Link state changed (called from driver)
		 */
		void link_state_changed()
		{
			if (_link_state_sigh.valid())
				Genode::Signal_transmitter(_link_state_sigh).submit();
		}

		/***************************
		 ** Nic-session interface **
		 ***************************/

		Mac_address mac_address() override
		{
			return _device.mac_address();
		}

		bool link_state() // override
		{
			return _device.link_state();
		}

		void link_state_sigh(Genode::Signal_context_capability sigh)
		{
			_link_state_sigh = sigh;
		}
};


/**
 * Root component, handling new session requests
 */
struct Nic::Root : Packet_root<Root_component, Session_component, Device, CACHED>
{
	Root(Server::Entrypoint &ep, Allocator *md_alloc, Device &device)
	: Packet_root(ep, md_alloc, device) { }
};

#endif /* _NIC__COMPONENT_H_ */
