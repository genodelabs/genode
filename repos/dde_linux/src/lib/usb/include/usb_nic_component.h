/*
 * \brief  Block-session implementation for network devices
 * \author Sebastian Sumpf
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _USB_NIC_COMPONENT_H_
#define _USB_NIC_COMPONENT_H_

#include <nic/component.h>
#include <root/component.h>

namespace Usb_nic {
	using namespace Genode;
	class  Session_component;
	struct Device;
};


struct Usb_nic::Device
{
	Session_component *_session;

	/**
	 * Transmit data to driver
	 */
	virtual bool tx(addr_t virt, size_t size) = 0;

	/**
	 * Return mac address of device
	 */
	virtual Nic::Mac_address mac_address() = 0;

	/**
	 * Return current link-state (true if link detected)
	 */
	virtual bool link_state() = 0;

	/**
	 * Set session belonging to this driver
	 */
	void session(Session_component *s) { _session = s; }

	/**
	 * Check for session
	 */
	bool session() { return _session != 0; }

	/**
	 * Alloc an SKB
	 */
	virtual sk_buff *alloc_skb() = 0;

	/**
	 * Submit SKB to device
	 */
	virtual void tx_skb(sk_buff *skb) = 0;

	/**
	 * Setup SKB with 'data' of 'size', return 'false' if SKB is longer than
	 * 'end'.
	 */
	virtual bool skb_fill(struct sk_buff *skb, unsigned char *data, Genode::size_t size, unsigned char *end) = 0;

	/**
	 * Call driver fixup function on SKB
	 */
	virtual void tx_fixup(struct sk_buff *skb) = 0;

	/**
	 * Return true if device supports burst operations
	 */
	virtual bool burst() = 0;

	Device() : _session(0) { }
};


class Usb_nic::Session_component : public Nic::Session_component
{
	private:

		Device *_device;    /* device this session is using */

	protected:

		void _send_burst()
		{
			static sk_buff work_skb; /* dummy skb for fixup calls */
			static Packet_descriptor save;

			sk_buff       *skb = nullptr;
			unsigned char *ptr = nullptr;

			/* submit received packets to lower layer */
			while (((_tx.sink()->packet_avail() || save.valid()) && _tx.sink()->ready_to_ack()))
			{
				/* alloc skb */
				if (!skb) {
					if (!(skb = _device->alloc_skb()))
						return;

					ptr           = skb->data;
					work_skb.data = nullptr;
				}

				Packet_descriptor packet = save.valid() ? save : _tx.sink()->get_packet();
				save                     = Packet_descriptor();

				if (!_device->skb_fill(&work_skb, ptr, packet.size(), skb->end)) {
					/* submit batch */
					_device->tx_skb(skb);
					skb  = nullptr;
					save = packet;
					continue;
				}

				/* copy packet to current data pos */
				Genode::memcpy(work_skb.data, _tx.sink()->packet_content(packet), packet.size());
				/* call fixup on dummy SKB */
				_device->tx_fixup(&work_skb);
				/* advance to next slot */
				ptr       = work_skb.end;
				skb->len += work_skb.truesize;
				/* acknowledge to client */
				_tx.sink()->acknowledge_packet(packet);
			}

			/* submit last skb */
			if (skb)
				_device->tx_skb(skb);
		}

		bool _send()
		{
			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Genode::Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.valid()) {
				PWRN("Invalid tx packet");
				return true;
			}

			bool ret = _device->tx((addr_t)_tx.sink()->packet_content(packet), packet.size());
			_tx.sink()->acknowledge_packet(packet);

			return ret;
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			if (_device->burst())
				_send_burst();
			else
				while (_send());
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::size_t const tx_buf_size,
		                  Genode::size_t const rx_buf_size,
		                  Genode::Allocator   &rx_block_md_alloc,
		                  Genode::Ram_session &ram_session,
		                  Server::Entrypoint  &ep,
		                  Device *device)
		: Nic::Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, ram_session, ep),
			_device(device)
			{ _device->session(this); }


		Nic::Mac_address mac_address() override { return _device->mac_address(); }
		bool link_state()              override { return _device->link_state(); }
		void link_state_changed()               { _link_state_changed(); }

		/**
		 * Send packet to client (called from driver)
		 */
		void rx(addr_t virt, size_t size)
		{
			_handle_packet_stream();

			if (!_rx.source()->ready_to_submit())
				return;

			try {
				Packet_descriptor p =_rx.source()->alloc_packet(size);
				Genode::memcpy(_rx.source()->packet_content(p), (void*)virt, size);
				_rx.source()->submit_packet(p);
			} catch (...) {
				/* drop */
				return;
			}
		}
};

/*
 * Shortcut for single-client root component
 */
typedef Genode::Root_component<Usb_nic::Session_component, Genode::Single_client> Root_component;

/**
 * Root component, handling new session requests
 */
class Root : public Root_component
{
	private:

		Server::Entrypoint &_ep;
		Usb_nic::Device    *_device;

	protected:

		Usb_nic::Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(Usb_nic::Session_component));
			if (ram_quota < session_size)
				throw Genode::Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers. Also check both sizes separately to handle a
			 * possible overflow of the sum of both sizes.
			 */
			if (tx_buf_size               > ram_quota - session_size
			 || rx_buf_size               > ram_quota - session_size
			 || tx_buf_size + rx_buf_size > ram_quota - session_size) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, tx_buf_size + rx_buf_size + session_size);
				throw Genode::Root::Quota_exceeded();
			}

			return new (Root::md_alloc())
			            Usb_nic::Session_component(tx_buf_size, rx_buf_size,
			                                       *env()->heap(),
			                                       *env()->ram_session(),
			                                       _ep, _device);
		}

	public:

		Root(Server::Entrypoint &ep, Genode::Allocator &md_alloc,
		     Usb_nic::Device *device)
		: Root_component(&ep.rpc_ep(), &md_alloc),
			_ep(ep), _device(device)
		{ }
};

#endif /* _USB_NIC_COMPONENT_H_ */
