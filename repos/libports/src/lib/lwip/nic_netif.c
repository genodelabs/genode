/*
 * \brief  LwIP netif for the Nic session
 * \author Emery Hemingway
 * \author Sebastian Sumpf
 * \date   2025-01-16
 *
 * Connects lwIP netif to nic_client C-API
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#if ETH_PAD_SIZE
#error ETH_PAD_SIZE defined but unsupported by lwip/nic_netif.h
#endif

/* local */
#include <arch/cc.h>
#include <nic_netif.h>
#include <string.h>

/* Genode */
#include <genode_c_api/nic_client.h>
#include <genode_c_api/socket.h>

/* lwip */
#include <lwip/dhcp.h>
#include <lwip/dns.h>
#include <lwip/netif.h>
#include <netif/etharp.h>
#if LWIP_IPV6
#include <lwip/ethip6.h>
#endif


struct genode_netif_handle
{
	struct netif               *netif;
	bool                        address_valid;
	bool                        address_configured;
	bool                        dhcp;
	ip_addr_t                   ip;
	ip_addr_t                   netmask;
	ip_addr_t                   gateway;
	ip_addr_t                   nameserver;
	struct genode_nic_client   *nic_handle;
};


struct genode_nic_client_tx_packet_context
{
	struct pbuf *pbuf;
};


static unsigned long
netif_tx_packet_content(struct genode_nic_client_tx_packet_context *ctx,
                        char *dst, unsigned long dst_len)
{
	struct pbuf *pbuf = ctx->pbuf;

	if (pbuf->tot_len > dst_len) {
		lwip_printf("error: pbuf larger (%zu) than packet (%lu)",
		            pbuf->tot_len, dst_len);
		return 0;
	}
	/*
	 * We iterate over the pbuf chain until we have read the entire
	 * pbuf into the packet.
	 */
	for (struct pbuf *p = pbuf; p != NULL; p = p->next) {
		char const *src = (char const *)p->payload;
		memcpy(dst, src, p->len);
		dst += p->len;
	}

	return pbuf->tot_len;
}


/*
 * Callback issued by lwIP to write a Nic packet
 */
static err_t nic_netif_linkoutput(struct netif *netif, struct pbuf *p)
{
	struct genode_netif_handle *handle = netif->state;
	struct genode_nic_client_tx_packet_context ctx = { .pbuf = p };
	bool   progress;

	progress = genode_nic_client_tx_packet(handle->nic_handle,
	                                       netif_tx_packet_content,
	                                       &ctx);
	if (!progress)
		return ERR_WOULDBLOCK;

	lwip_genode_socket_schedule_peer();

	LINK_STATS_INC(link.xmit);
	return ERR_OK;
}


/*
 * Callback issued by lwIP to initialize netif struct
 */
static err_t nic_netif_init(struct netif *netif)
{
	struct genode_mac_address mac;
	struct genode_netif_handle *handle = netif->state;

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "";
#endif /* LWIP_NETIF_HOSTNAME */

	netif->name[0] = 'e';
	netif->name[1] = 'n';

	netif->output = etharp_output;
#if LWIP_IPV6
	netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

	netif->linkoutput = nic_netif_linkoutput;

	/* Set physical MAC address */
	mac = genode_nic_client_mac_address(handle->nic_handle);
	for(int i=0; i < 6; ++i)
		netif->hwaddr[i] = mac.addr[i];

	netif->mtu        = 1500;
	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	netif->flags      = NETIF_FLAG_BROADCAST |
	                    NETIF_FLAG_ETHARP    |
	                    NETIF_FLAG_LINK_UP;

	return ERR_OK;
}


static char *
_ip4raw_ntoa(unsigned addr, char *buf, int buflen)
{
	ip_addr_t ip = IPADDR4_INIT(addr);

	return ip4addr_ntoa_r(ip_2_ip4(&ip), buf, buflen);
}


static void  nic_netif_status_callback(struct netif *netif)
{
	struct genode_netif_handle *handle =netif->state;
	if (netif_is_up(netif)) {
		if (IP_IS_V6_VAL(netif->ip_addr)) {
			lwip_printf("lwIP Nic interface up, address=%s",
			            ip6addr_ntoa(netif_ip6_addr(netif, 0)));
			handle->address_configured = true;
		} else if (!ip4_addr_isany(netif_ip4_addr(netif))) {

			handle->address_configured = true;

			char ip_addr[IPADDR_STRLEN_MAX];
			char netmask[IPADDR_STRLEN_MAX];
			char gateway[IPADDR_STRLEN_MAX];
			char nameserver[IPADDR_STRLEN_MAX];

			struct genode_socket_info info;
			lwip_genode_netif_info(handle, &info);

			_ip4raw_ntoa(info.ip_addr,    ip_addr,    IPADDR_STRLEN_MAX);
			_ip4raw_ntoa(info.netmask,    netmask,    IPADDR_STRLEN_MAX);
			_ip4raw_ntoa(info.gateway,    gateway,    IPADDR_STRLEN_MAX);
			_ip4raw_ntoa(info.nameserver, nameserver, IPADDR_STRLEN_MAX);

			lwip_printf("lwIP Nic interface up address=%s netmask=%s gateway=%s"
			            " nameserver=%s", ip_addr, netmask, gateway, nameserver);
		}
	} else {
			lwip_printf("lwIP Nic interface down");
			handle->address_configured = false;
	}

	//XXX: add callback
	//nic_netif->status_callback();
}


/*
 * public functions of this module
 */

struct genode_netif_handle *lwip_genode_netif_init(char const *label)
{
	ip4_addr_t v4dummy;
	IP4_ADDR(&v4dummy, 0, 0, 0, 0);
	struct netif *net = NULL;
	struct genode_netif_handle *handle;

	net = mem_malloc(sizeof(struct netif));
	if (net == NULL) {
		lwip_printf("error: failer to allocate Nic for lwIP interface");
		return NULL;
	}

	memset(net, 0x00, sizeof(struct netif));

	handle = mem_malloc(sizeof(struct genode_netif_handle));
	if (handle == NULL) {
		lwip_printf("error: failed to allocate Nic handle");
		mem_free(net);
		return NULL;
	}

	handle->nic_handle         = genode_nic_client_create(label);
	handle->netif              = net;
	handle->address_valid      = false;
	handle->address_configured = false;

	net = netif_add(net, &v4dummy, &v4dummy, &v4dummy,
	                handle, nic_netif_init, ethernet_input);
	if (net == NULL) {
		lwip_printf("error: failed to initialize Nic to lwIP interface");
		mem_free(net);
		mem_free(handle);
		return NULL;
	}

	netif_set_default(net);
	netif_set_status_callback(net, nic_netif_status_callback);
	nic_netif_status_callback(net);

	return handle;
}


static bool nic_netif_address_static(struct genode_netif_handle  *handle,
                                     struct genode_socket_config *config)
{
	if (config->ip_addr) {
		if(!ipaddr_aton(config->ip_addr, &handle->ip)) {
			lwip_printf("error: invalid ip address: %s\n", config->ip_addr);
			return false;
		}
	}

	if (config->netmask)
		ipaddr_aton(config->netmask, &handle->netmask);

	if (config->gateway)
		ipaddr_aton(config->gateway, &handle->gateway);

	netif_set_addr(handle->netif, ip_2_ip4(&handle->ip), ip_2_ip4(&handle->netmask),
	               ip_2_ip4(&handle->gateway));

	if (config->nameserver) {
		ipaddr_aton(config->nameserver, &handle->nameserver);
		dns_setserver(0, &handle->nameserver);
	}

	return true;
}


void lwip_genode_netif_address(struct genode_netif_handle  *handle,
                               struct genode_socket_config *config)
{
	if (!handle || !config) {
		lwip_printf("error: %s invalid args", __func__);
		return;
	}

	if (!handle->netif) {
		lwip_printf("error: %s no network interface", __func__);
		return;
	}

	netif_set_up(handle->netif);

	if (config->dhcp)
		handle->dhcp = true;
	else {
		handle->dhcp = false;

		if (!nic_netif_address_static(handle, config)) {
			netif_set_down(handle->netif);
			return;
		}
	}

	handle->address_valid = true;
	lwip_genode_netif_link_state(handle);
}


void lwip_genode_netif_info(struct genode_netif_handle  *handle,
                            struct genode_socket_info   *info)
{
	struct netif *netif;
	ip_addr_t const *dns = NULL;

	if (!handle || !lwip_genode_netif_configured(handle)) return;

	netif = handle->netif;

	info->ip_addr    = ip4_addr_get_u32(netif_ip4_addr(netif));
	info->netmask    = ip4_addr_get_u32(netif_ip4_netmask(netif));
	info->gateway    = ip4_addr_get_u32(netif_ip4_gw(netif));
	info->link_state = genode_nic_client_link_state(handle->nic_handle);

	dns = dns_getserver(0);
	if (dns)
		info->nameserver = ip4_addr_get_u32(ip_2_ip4(dns));

	/* default to server gateway address */
	if (!info->nameserver) info->nameserver = info->gateway;
}


void lwip_genode_netif_mtu(struct genode_netif_handle *handle, unsigned mtu)
{
	if (!handle || !handle->netif) return;
	handle->netif->mtu = mtu ? mtu : 1500;
}


bool lwip_genode_netif_configured(struct genode_netif_handle *handle)
{
	return handle->address_configured;
}


void lwip_genode_netif_link_state(struct genode_netif_handle *handle)
{
	err_t err;
	bool up;

	if (!handle || !handle->address_valid) return;

	/*
	 * if the application wants to be informed of the
	 * link state then it should use 'set_link_callback'
	 */
	up = genode_nic_client_link_state(handle->nic_handle);
	if (up) {
		netif_set_link_up(handle->netif);
		if (handle->dhcp) {
			err = dhcp_start(handle->netif);
			if (err != ERR_OK)
				lwip_printf("error: failed to configure lwIP interface with DHCP, "
				            " error %d", -err);
		} else dhcp_inform(handle->netif);
	} else {
		netif_set_link_down(handle->netif);
		if (handle->dhcp) dhcp_release_and_stop(handle->netif);
	}
}


struct genode_nic_client_rx_context
{
	struct netif *netif;
};


static genode_nic_client_rx_result_t
netif_rx_one_packet(struct genode_nic_client_rx_context *ctx,
                    char const *ptr, unsigned long len)
{
	err_t err;
	struct pbuf  *p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
	if (!p) return GENODE_NIC_CLIENT_RX_REJECTED;

	LINK_STATS_INC(link.recv);

	memcpy(p->payload, ptr, len);

	if ((err = ctx->netif->input(p, ctx->netif)) != ERR_OK) {
		lwip_printf("error: forwarding Nic packet to lwIP (%d)", err);
		return GENODE_NIC_CLIENT_RX_RETRY;
	}

	return GENODE_NIC_CLIENT_RX_ACCEPTED;
}


void lwip_genode_netif_rx(struct genode_netif_handle *handle)
{
	struct genode_nic_client_rx_context ctx = { .netif = handle->netif };
	bool progress = false;

	if (!handle) return;

	while (genode_nic_client_rx(handle->nic_handle, netif_rx_one_packet, &ctx))
		progress = true;

	if (progress) lwip_genode_socket_schedule_peer();
}
