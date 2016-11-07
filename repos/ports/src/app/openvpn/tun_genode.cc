/**
 * \brief  TUN/TAP to Nic_session interface
 * \author Josef Soentgen
 * \date   2014-06-05
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/snprintf.h>
#include <cap_session/connection.h>
#include <nic_session/rpc_object.h>
#include <os/server.h>
#include <root/component.h>
#include <util/string.h>

/* local includes */
#include "tuntap.h"

/* OpenVPN includes */
extern "C" {
#include "config.h"
#include "syshead.h"
#include "socket.h"
#include "tun.h"
}


extern Tuntap_device *tuntap_dev();


static in_addr_t gen_broadcast_addr(in_addr_t local, in_addr_t netmask) {
	return local | ~netmask; }


extern "C" void open_tun(char const *dev, char const *dev_type,
                         char const *dev_node, struct tuntap *tt)
{
	/* start with a failed attempt to open tun/tap device */
	tt->fd = -1;

	if (tt->ipv6) {
		Genode::error("IPv6 is currently not supported!");
		return;
	}

	if (tt->type == DEV_TYPE_NULL) {
		Genode::error("null device not supported");
		return;
	}

	char name[256];
	Genode::snprintf(name, sizeof (name), "/dev/%s", dev);

	tt->actual_name = string_alloc(name, NULL);
	tt->fd = tuntap_dev()->fd();
}


extern "C" void close_tun(struct tuntap *tt)
{
	free(tt->actual_name);
	free(tt);
}


extern "C" int write_tun(struct tuntap *tt, uint8_t *buf, int len)
{
	if (len <= 0)
		return -1;

	switch (tt->type) {
	case DEV_TYPE_TAP:
		return tuntap_dev()->write(reinterpret_cast<char const*>(buf), len);
		break;
	case DEV_TYPE_TUN:
		break;
	}

	return -1;
}


extern "C" int read_tun(struct tuntap *tt, uint8_t *buf, int len)
{
	if (len <= 0)
		return -1;

	{
		/* read from fd to prevent select() from triggering more than once */
		char tmp[1];
		::read(tt->fd, tmp, sizeof (tmp));
	}

	switch (tt->type) {
	case DEV_TYPE_TAP:
		return tuntap_dev()->read(reinterpret_cast<char*>(buf), len);
		break;
	case DEV_TYPE_TUN:
		break;
	}

	return -1;
}


extern "C" void tuncfg(char const *dev, char const *dev_type,
                       char const *dev_node, int persist_mode,
                       char const *username, char const *groupname,
                       struct tuntap_options const *options) { }


extern "C" char const *guess_tuntap_dev(char const *dev, char const *dev_type,
                                        char const *dev_node, struct gc_arena *gc)
{
	return dev;
}


extern "C" struct tuntap *init_tun(char const *dev, char const *dev_type,
                                   int topology, char const *ifconfig_local_parm,
                                   char const *ifconfig_remote_netmask_parm,
                                   char const *ifconfig_ipv6_local_parm,
                                   int ifconfig_ipv6_netbits_parm,
                                   char const *ifconfig_ipv6_remote_parm,
                                   in_addr_t local_public, in_addr_t remote_public,
                                   bool const strict_warn, struct env_set *es)
{
	struct tuntap *tt;

	ALLOC_OBJ(tt, struct tuntap);
	Genode::memset(tt, 0, sizeof (struct tuntap));

	tt->fd       = -1;
	tt->ipv6     = false;
	tt->type     = dev_type_enum(dev, dev_type);
	tt->topology = topology;

	if (ifconfig_local_parm && ifconfig_remote_netmask_parm) {
		bool tun = is_tun_p2p(tt);

		tt->local = getaddr(GETADDR_RESOLVE | GETADDR_HOST_ORDER |
		                    GETADDR_FATAL_ON_SIGNAL | GETADDR_FATAL,
		                    ifconfig_local_parm, 0, NULL, NULL);

		tt->remote_netmask = getaddr((tun ? GETADDR_RESOLVE : 0) |
		                             GETADDR_HOST_ORDER | GETADDR_FATAL_ON_SIGNAL |
		                             GETADDR_FATAL, ifconfig_remote_netmask_parm,
		                             0, NULL, NULL);

		if (!tun) {
			tt->broadcast = gen_broadcast_addr(tt->local, tt->remote_netmask);
		}

		tt->did_ifconfig_setup = true;
	}

	return tt;
}


extern "C" void init_tun_post(struct tuntap *tt, struct frame const *frame,
                              struct tuntap_options const *options) { }


extern "C" void do_ifconfig(struct tuntap *tt, char const *actual_name,
                            int tun_mtu, struct env_set const *es)
{
	/**
	 * After OpenVPN has received a PUSH_REPLY it will configure
	 * the TUN/TAP device by calling this function. At this point
	 * it is save to actually announce the Nic_session. Therefore,
	 * we release the lock.
	 */
	tuntap_dev()->up();
}


extern "C" bool is_dev_type(char const *dev, char const *dev_type,
                            char const *match_type)

{
	if (!dev)
		return false;

	if (dev_type)
		return !Genode::strcmp(dev_type, match_type);
	else
		return !Genode::strcmp(dev, match_type, Genode::strlen(match_type));
}


extern "C" int dev_type_enum(char const *dev, char const *dev_type)
{
	if (is_dev_type(dev, dev_type, "tap"))
		return DEV_TYPE_TAP;

	if (is_dev_type(dev, dev_type, "tun"))
		return DEV_TYPE_TUN;

	if (is_dev_type(dev, dev_type, "null"))
		return DEV_TYPE_NULL;

	return DEV_TYPE_UNDEF;
}


extern "C" char const *dev_type_string(char const *dev, char const *dev_type)
{
	switch (dev_type_enum(dev, dev_type)) {
	case DEV_TYPE_TAP:
		return "tap";
	case DEV_TYPE_TUN:
		return "tun";
	case DEV_TYPE_NULL:
		return "null";
	default:
		return "[unknown-dev-type]";
	}
}


extern "C" char const *ifconfig_options_string(struct tuntap const* tt,
                                               bool remote, bool disable,
                                               struct gc_arena *gc)
{
	return 0;
}


extern "C" bool is_tun_p2p(struct tuntap const *tt)
{
	bool tun = false;

	if (tt->type == DEV_TYPE_TAP ||
	    (tt->type == DEV_TYPE_TUN && tt->topology == TOP_SUBNET))
		tun = false;
	else if (tt->type == DEV_TYPE_TUN)
		tun = true;
	else
		Genode::error("problem with tun vs. tap setting");

	return tun;
}


extern "C" void check_subnet_conflict(const in_addr_t, const in_addr_t,
                                      char const *) { }


extern "C" void warn_on_use_of_common_subnets(void) { }


extern "C" char const *tun_stat(struct tuntap const *tt, unsigned rwflags,
                                struct gc_arena *gc)
{
	struct buffer out = alloc_buf_gc(64, gc);
	if (tt) {
		if (rwflags & EVENT_READ) {
			buf_printf(&out, "T%s", (tt->rwflags_debug & EVENT_READ) ? "R" : "r");
		}
		if (rwflags & EVENT_WRITE) {
			buf_printf(&out, "T%s", (tt->rwflags_debug & EVENT_WRITE) ? "W" : "w");
		}
	}
	else
		buf_printf(&out, "T?");

	return buf_str(&out);
}
