/*
 * \brief  Block-session implementation for network devices
 * \author Sebastian Sumpf
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _USB_NIC_COMPONENT_H_
#define _USB_NIC_COMPONENT_H_

/* Genode includes */
#include <base/log.h>
#include <nic/component.h>
#include <root/component.h>

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>
#include <lx_emul/extern_c_end.h>

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>

namespace Usb_nic {

	using namespace Genode;
	using Genode::size_t;

	class  Session_component;
	struct Device;
};


class Usb_network_session
{
	protected:

		Usb_nic::Device &_device;

	public:

		Usb_network_session(Usb_nic::Device &device)
		:
			_device { device }
		{ }

		virtual void link_state_changed() = 0;

		virtual void rx(Genode::addr_t virt,
		                Genode::size_t size) = 0;
};


struct Usb_nic::Device
{
	Usb_network_session *_session;

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
	void session(Usb_network_session *s) { _session = s; }

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


class Usb_nic::Session_component : public Usb_network_session,
                                   public Nic::Session_component
{
	protected:

		void _send_burst()
		{
			static sk_buff work_skb; /* dummy skb for fixup calls */
			static Packet_descriptor save;

			sk_buff       *skb = nullptr;
			unsigned char *ptr = nullptr;

			/* submit received packets to lower layer */
			while (((_tx.sink()->packet_avail() || save.size()) && _tx.sink()->ready_to_ack()))
			{
				/* alloc skb */
				if (!skb) {
					if (!(skb = _device.alloc_skb()))
						return;

					ptr           = skb->data;
					work_skb.data = nullptr;
				}

				Packet_descriptor packet = save.size() ? save : _tx.sink()->get_packet();
				save                     = Packet_descriptor();

				if (!_device.skb_fill(&work_skb, ptr, packet.size(), skb->end)) {
					/* submit batch */
					_device.tx_skb(skb);
					skb  = nullptr;
					save = packet;
					continue;
				}

				/* copy packet to current data pos */
				try { Genode::memcpy(work_skb.data, _tx.sink()->packet_content(packet), packet.size()); }
				catch (Genode::Packet_descriptor::Invalid_packet) { }
				/* call fixup on dummy SKB */
				_device.tx_fixup(&work_skb);
				/* advance to next slot */
				ptr       = work_skb.end;
				skb->len += work_skb.truesize;
				/* acknowledge to client */
				_tx.sink()->acknowledge_packet(packet);
			}

			/* submit last skb */
			if (skb)
				_device.tx_skb(skb);
		}

		bool _send()
		{
			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Genode::Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.size() || !_tx.sink()->packet_valid(packet)) {
				Genode::warning("Invalid tx packet");
				return true;
			}

			bool ret = _device.tx((addr_t)_tx.sink()->packet_content(packet), packet.size());
			_tx.sink()->acknowledge_packet(packet);

			return ret;
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			if (_device.burst())
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
		                  Genode::Env         &env,
		                  Device              &device)
		:
			Usb_network_session    { device },
			Nic::Session_component { tx_buf_size, rx_buf_size, Genode::CACHED,
			                         rx_block_md_alloc, env }
		{
			_device.session(this);
		}


		/****************************
		 ** Nic::Session_component **
		 ****************************/

		Nic::Mac_address mac_address() override { return _device.mac_address(); }
		bool link_state()              override { return _device.link_state(); }


		/*************************
		 ** Usb_network_session **
		 *************************/

		void link_state_changed() override { _link_state_changed(); }

		/**
		 * Send packet to client (called from driver)
		 */
		void rx(addr_t virt, size_t size) override
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

		Genode::Env     &_env;
		Usb_nic::Device &_device;

	protected:

		Usb_nic::Session_component *_create_session(const char *args)
		{
			using namespace Genode;
			using Genode::size_t;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(Usb_nic::Session_component));
			if (ram_quota < session_size)
				throw Genode::Insufficient_ram_quota();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, " need %ld",
				              tx_buf_size + rx_buf_size + session_size);
				throw Genode::Insufficient_ram_quota();
			}

			return new (Root::md_alloc())
				Usb_nic::Session_component(tx_buf_size, rx_buf_size,
				                           Lx::Malloc::mem(), _env,
				                           _device);
		}

	public:

		Root(Genode::Env       &env,
		     Genode::Allocator &md_alloc,
		     Usb_nic::Device   &device)
		:
			Root_component { &env.ep().rpc_ep(), &md_alloc },
			_env           { env },
			_device        { device }
		{ }
};


namespace Genode {

	class Uplink_client;
}


class Genode::Uplink_client : public Usb_network_session,
                              public Uplink_client_base
{
	private:

		sk_buff        _burst_work_skb { };
		sk_buff       *_burst_skb      { nullptr };
		unsigned char *_burst_ptr      { nullptr };


		/************************
		 ** Uplink_client_base **
		 ************************/

		Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) override
		{
			if (_device.tx((addr_t)conn_rx_pkt_base, conn_rx_pkt_size) == 0) {
				return Transmit_result::ACCEPTED;
			}
			return Transmit_result::REJECTED;
		}

		void _drv_transmit_pkt_burst_prepare() override
		{
			_burst_skb = nullptr;
			_burst_ptr = nullptr;
		}

		Burst_result
		_drv_transmit_pkt_burst_step(Packet_descriptor const &packet,
		                             char              const *packet_base,
		                             Packet_descriptor       &save) override
		{
			/* alloc _burst_skb */
			if (!_burst_skb) {
				if (!(_burst_skb = _device.alloc_skb()))
					return Burst_result::BURST_FAILED;

				_burst_ptr           = _burst_skb->data;
				_burst_work_skb.data = nullptr;
			}

			if (!_device.skb_fill(&_burst_work_skb, _burst_ptr, packet.size(), _burst_skb->end)) {

				/* submit batch */
				_device.tx_skb(_burst_skb);
				_burst_skb  = nullptr;
				save = packet;
				return Burst_result::BURST_CONTINUE;
			}

			/* copy packet to current data pos */
			Genode::memcpy(_burst_work_skb.data, packet_base, packet.size());

			/* call fixup on dummy SKB */
			_device.tx_fixup(&_burst_work_skb);

			/* advance to next slot */
			_burst_ptr       = _burst_work_skb.end;
			_burst_skb->len += _burst_work_skb.truesize;

			return Burst_result::BURST_SUCCEEDED;
		}

		void _drv_transmit_pkt_burst_finish() override
		{
			/* submit last _burst_skb */
			if (_burst_skb)
				_device.tx_skb(_burst_skb);
		}

		bool _drv_supports_transmit_pkt_burst() override
		{
			return _device.burst();
		}

	public:

		Uplink_client(Env             &env,
		              Allocator       &alloc,
		              Usb_nic::Device &device)
		:
			Usb_network_session { device },
			Uplink_client_base  { env, alloc, _device.mac_address() }
		{
			_device.session(this);
			_drv_handle_link_state(_device.link_state());
		}


		/*************************
		 ** Usb_network_session **
		 *************************/

		void link_state_changed() override
		{
			_drv_handle_link_state(_device.link_state());
		}

		void rx(Genode::addr_t virt,
		        Genode::size_t size) override
		{
			_drv_rx_handle_pkt(
				size,
				[&] (void   *conn_tx_pkt_base,
					 size_t &)
			{
				memcpy(conn_tx_pkt_base, (void *)virt, size);
				return Write_result::WRITE_SUCCEEDED;
			});
		}
};

#endif /* _USB_NIC_COMPONENT_H_ */
