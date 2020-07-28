/*
 * \brief  LwIP netif for the Nic session
 * \author Emery Hemingway
 * \date   2016-09-28
 *
 * If you want to use the lwIP API in a native Genode
 * component then this is the Nic client to use.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__NIC_NETIF_H__
#define __LWIP__NIC_NETIF_H__

#if ETH_PAD_SIZE
#error ETH_PAD_SIZE defined but unsupported by lwip/nic_netif.h
#endif

#ifndef __cplusplus
#error lwip/nic_netif.h is a C++ only header
#endif

/* Genode includes */
#include <lwip/genode_init.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <base/log.h>

namespace Lwip {

extern "C" {
/* LwIP includes */
#include <lwip/netif.h>
#include <netif/etharp.h>
#if LWIP_IPV6
#include <lwip/ethip6.h>
#endif
#include <lwip/init.h>
#include <lwip/dhcp.h>
#include <lwip/dns.h>
}

	class Nic_netif;

	extern "C" {

		static void nic_netif_pbuf_free(pbuf *p);
		static err_t nic_netif_init(struct netif *netif);
		static err_t nic_netif_linkoutput(struct netif *netif, struct pbuf *p);
		static void  nic_netif_status_callback(struct netif *netif);
	}

	/**
	 * Metadata for packet backed pbufs
	 */
	struct Nic_netif_pbuf
	{
		struct pbuf_custom p { };
		Nic_netif &netif;
		Nic::Packet_descriptor packet;

		Nic_netif_pbuf(Nic_netif &nic, Nic::Packet_descriptor &pkt)
		: netif(nic), packet(pkt)
		{
			p.custom_free_function = nic_netif_pbuf_free;
		}
	};
}


class Lwip::Nic_netif
{
	private:

		enum {
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = 128 * PACKET_SIZE,
		};

		Genode::Tslab<Nic_netif_pbuf, 128*sizeof(Nic_netif_pbuf)> _pbuf_alloc;

		Nic::Packet_allocator _nic_tx_alloc;
		Nic::Connection _nic;

		struct netif _netif { };

		ip_addr_t ip { };
		ip_addr_t nm { };
		ip_addr_t gw { };

		Genode::Io_signal_handler<Nic_netif> _link_state_handler;
		Genode::Io_signal_handler<Nic_netif> _rx_packet_handler;

		bool _dhcp { false };

	public:

		void free_pbuf(Nic_netif_pbuf &pbuf)
		{
			if (!_nic.rx()->ready_to_ack()) {
				Genode::error("Nic rx acknowledgement queue congested, blocking to  free pbuf");
			}

			_nic.rx()->acknowledge_packet(pbuf.packet);
			destroy(_pbuf_alloc, &pbuf);
		}


		/*************************
		 ** Nic signal handlers **
		 *************************/

		void handle_link_state()
		{
			/*
			 * if the application wants to be informed of the
			 * link state then it should use 'set_link_callback'
			 */
			if (_nic.link_state()) {
				netif_set_link_up(&_netif);
				if (_dhcp) {
					err_t err = dhcp_start(&_netif);
					if (err != ERR_OK) {
						Genode::error("failed to configure lwIP interface with DHCP, error ", -err);
					}
				} else {
					dhcp_inform(&_netif);
				}
			} else {
				netif_set_link_down(&_netif);
				if (_dhcp) {
					dhcp_release_and_stop(&_netif);
				}
			}
		}

		void handle_rx_packets()
		{
			Genode::Mutex::Guard guard { Lwip::mutex() };
			auto &rx = *_nic.rx();

			while (rx.packet_avail() && rx.ready_to_ack()) {

				Nic::Packet_descriptor packet = rx.get_packet();

				Nic_netif_pbuf *nic_pbuf = new (_pbuf_alloc)
					Nic_netif_pbuf(*this, packet);

				pbuf* p = pbuf_alloced_custom(
					PBUF_RAW,
					packet.size(),
					PBUF_REF,
					&nic_pbuf->p,
					rx.packet_content(packet),
					packet.size());
				LINK_STATS_INC(link.recv);

				if (_netif.input(p, &_netif) != ERR_OK) {
					Genode::error("error forwarding Nic packet to lwIP");
					pbuf_free(p);
				}
			}
		}

		void configure(Genode::Xml_node const &config)
		{
			_dhcp = config.attribute_value("dhcp", false);

			typedef Genode::String<IPADDR_STRLEN_MAX> Str;
			Str ip_str = config.attribute_value("ip_addr", Str());

			if (_dhcp && ip_str != "") {
				_dhcp = false;
				netif_set_down(&_netif);
				Genode::error("refusing to configure lwIP interface with both DHCP and a static IPv4 address");
				return;
			}

			netif_set_up(&_netif);

			if (ip_str != "") {
				ip_addr_t ipaddr;
				if (!ipaddr_aton(ip_str.string(), &ipaddr)) {
					Genode::error("lwIP configured with invalid IP address '",ip_str,"'");
					throw ip_str;
				}

				netif_set_ipaddr(&_netif, ip_2_ip4(&ipaddr));

				if (config.has_attribute("netmask")) {
					Str str = config.attribute_value("netmask", Str());
					ip_addr_t ip;
					ipaddr_aton(str.string(), &ip);
					netif_set_netmask(&_netif, ip_2_ip4(&ip));
				}

				if (config.has_attribute("gateway")) {
					Str str = config.attribute_value("gateway", Str());
					ip_addr_t ip;
					ipaddr_aton(str.string(), &ip);
					netif_set_gw(&_netif, ip_2_ip4(&ip));
				}

			}

			if (config.has_attribute("nameserver")) {
				/*
				 * LwIP does not use DNS internally, but the application
				 * should expect "dns_getserver" to work regardless of
				 * how the netif configures itself.
				 */
				Str str = config.attribute_value("nameserver", Str());
				ip_addr_t ip;
				ipaddr_aton(str.string(), &ip);
				dns_setserver(0, &ip);
			}

			handle_link_state();
		}

		Nic_netif(Genode::Env &env,
		          Genode::Allocator &alloc,
		          Genode::Xml_node config)
		:
			_pbuf_alloc(alloc), _nic_tx_alloc(&alloc),
			_nic(env, &_nic_tx_alloc,
			     BUF_SIZE, BUF_SIZE,
			     config.attribute_value("label", Genode::String<160>("lwip")).string()),
			_link_state_handler(env.ep(), *this, &Nic_netif::handle_link_state),
			_rx_packet_handler( env.ep(), *this, &Nic_netif::handle_rx_packets)
		{
			Genode::memset(&_netif, 0x00, sizeof(_netif));

			{
				ip4_addr_t v4dummy;
				IP4_ADDR(&v4dummy, 0, 0, 0, 0);

				netif* r = netif_add(&_netif, &v4dummy, &v4dummy, &v4dummy,
				                     this, nic_netif_init, ethernet_input);
				if (r == NULL) {
					Genode::error("failed to initialize Nic to lwIP interface");
					throw r;
				}
			}

			netif_set_default(&_netif);
			netif_set_status_callback(
				&_netif, nic_netif_status_callback);
			nic_netif_status_callback(&_netif);

			configure(config);
		}

		virtual ~Nic_netif() { }

		Lwip::netif& lwip_netif() { return _netif; }

		/**
		* Status callback to override in subclass
		 */
		virtual void status_callback() { }

		/**
		 * Callback issued by lwIP to initialize netif struct
		 *
		 * \noapi
		 */
		err_t init()
		{
			/*
			 * XXX: hostname and MTU could probably be
			 * set in the Nic client constructor
			 */

#if LWIP_NETIF_HOSTNAME
			/* Initialize interface hostname */
			_netif.hostname = "";
#endif /* LWIP_NETIF_HOSTNAME */

			_netif.name[0] = 'e';
			_netif.name[1] = 'n';

			_netif.output = etharp_output;
#if LWIP_IPV6
			_netif.output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

			_netif.linkoutput      = nic_netif_linkoutput;

			/* Set physical MAC address */
			Nic::Mac_address const mac = _nic.mac_address();
			for(int i=0; i<6; ++i)
				_netif.hwaddr[i] = mac.addr[i];

			_netif.mtu        = 1500; /* XXX: just a guess */
			_netif.hwaddr_len = ETHARP_HWADDR_LEN;
			_netif.flags      = NETIF_FLAG_BROADCAST |
			                    NETIF_FLAG_ETHARP    |
			                    NETIF_FLAG_LINK_UP;

			/* set Nic session signal handlers */
			_nic.link_state_sigh(_link_state_handler);
			_nic.rx_channel()->sigh_packet_avail(_rx_packet_handler);
			_nic.rx_channel()->sigh_ready_to_ack(_rx_packet_handler);

			return ERR_OK;
		}

		/**
		 * Callback issued by lwIP to write a Nic packet
		 *
		 * \noapi
		 */
		err_t linkoutput(struct pbuf *p)
		{
			auto &tx = *_nic.tx();

			/* flush acknowledgements */
			while (tx.ack_avail())
				tx.release_packet(tx.get_acked_packet());

			if (!tx.ready_to_submit()) {
				Genode::error("lwIP: Nic packet queue congested, cannot send packet");
				return ERR_WOULDBLOCK;
			}

			Nic::Packet_descriptor packet;
			try { packet = tx.alloc_packet(p->tot_len); }
			catch (...) {
				Genode::error("lwIP: Nic packet allocation failed, cannot send packet");
				return ERR_WOULDBLOCK;
			}

			/*
			 * We iterate over the pbuf chain until we have read the entire
			 * pbuf into the packet.
			 */
			char *dst = tx.packet_content(packet);
			for(struct pbuf *q = p; q != 0; q = q->next) {
				char const *src = (char*)q->payload;
				Genode::memcpy(dst, src, q->len);
				dst += q->len;
			}

			tx.submit_packet(packet);
			LINK_STATS_INC(link.xmit);
			return ERR_OK;
		}

		bool ready()
		{
			return netif_is_up(&_netif) &&
				!ip_addr_isany(&_netif.ip_addr);
		}
};


/**************************
 ** LwIP netif callbacks **
 **************************/

namespace Lwip {
	extern "C" {

/**
 * Free a packet buffer backed pbuf
 */
static void nic_netif_pbuf_free(pbuf *p)
{
	Nic_netif_pbuf *nic_pbuf = reinterpret_cast<Nic_netif_pbuf*>(p);
	nic_pbuf->netif.free_pbuf(*nic_pbuf);
}


/**
 * Initialize the netif
 */
static err_t nic_netif_init(struct netif *netif)
{
	Lwip::Nic_netif *nic_netif = (Lwip::Nic_netif *)netif->state;
	return nic_netif->init();
}


/**
 * Send a raw packet to the Nic session
 */
static err_t nic_netif_linkoutput(struct netif *netif, struct pbuf *p)
{
	Lwip::Nic_netif *nic_netif = (Lwip::Nic_netif *)netif->state;
	return nic_netif->linkoutput(p);
}


static void nic_netif_status_callback(struct netif *netif)
{
	Lwip::Nic_netif *nic_netif = (Lwip::Nic_netif *)netif->state;

	if (netif_is_up(netif)) {
		if (IP_IS_V6_VAL(netif->ip_addr)) {
			Genode::log("lwIP Nic interface up"
			            ", address=",(char const*)ip6addr_ntoa(netif_ip6_addr(netif, 0)));
		} else if (!ip4_addr_isany(netif_ip4_addr(netif))) {
			typedef Genode::String<IPADDR_STRLEN_MAX> Str;
			Str address((char const*)ip4addr_ntoa(netif_ip4_addr(netif)));
			Str netmask((char const*)ip4addr_ntoa(netif_ip4_netmask(netif)));
			Str gateway((char const*)ip4addr_ntoa(netif_ip4_gw(netif)));

			Genode::log("lwIP Nic interface up"
				" address=", address,
				" netmask=", netmask,
				" gateway=", gateway);
		}
	} else {
			Genode::log("lwIP Nic interface down");
	}

	nic_netif->status_callback();
}

	}
}

#endif /* __LWIP__NIC_NETIF_H__ */
