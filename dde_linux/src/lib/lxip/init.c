/**
 * \brief  IP stack initialization
 * \author Sebastian Sumpf
 * \date   2013-08-26
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <dde_kit/timer.h>
#include <linux/inetdevice.h>
#include <net/tcp.h>
#include <init.h>
/*
 * Header declarations and tuning
 */
struct net init_net;

unsigned long *sysctl_local_reserved_ports;
struct pernet_operations loopback_net_ops;


/**
 * nr_free_buffer_pages - count number of pages beyond high watermark
 *
 * nr_free_buffer_pages() counts the number of pages which are beyond the
 * high
 * watermark within ZONE_DMA and ZONE_NORMAL.
 */
unsigned long nr_free_buffer_pages(void)
{
	return 1000;
}


/**
 * Initialize sub-systems
 */
int lxip_init()
{
	/* init data */
	INIT_LIST_HEAD(&init_net.dev_base_head);


	/*start jiffies */
	dde_kit_timer_init(0, 0);

	/* call __setup stuff */
	__ip_auto_config_setup("dhcp");

	core_sock_init();
	core_netlink_proto_init();

	/* sub-systems */
	subsys_net_dev_init();
	fs_inet_init();

	/* enable local accepts */
	IPV4_DEVCONF_ALL(&init_net, ACCEPT_LOCAL) = 0x1;

	/* congestion control */
	module_cubictcp_register();

	/* driver */
	module_driver_init();

	/* late  */
	late_tcp_congestion_default();

	/* dhcp */
	late_ip_auto_config();

	return 1;
}

