/*
 * \brief  Ioctl functions
 * \author Josef Soentgen
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* compiler includes */
#include <stdarg.h>

/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <sys/sockio.h>
#include <sys/socket.h>

/* local includes */
#include <wifi/socket_call.h>
#include <lx_user_emul.h>


extern Wifi::Socket_call socket_call;


extern "C" {

int ioctl(int fd, unsigned long request, ...)
{
	long arg;
	va_list ap;
	va_start(ap, request);
	arg = va_arg(ap, long);
	va_end(ap);

	struct ifreq *ifr = (struct ifreq *)(arg);

	switch (request) {
	case SIOCGIFADDR:
		Genode::error("ioctl: request SIOCGIFADDR not implemented.");
		return -1;
	case SIOCGIFINDEX:
		ifr->ifr_ifindex = 1;
		return 0;
	case SIOCGIFHWADDR:
		socket_call.get_mac_address((unsigned char*)ifr->ifr_hwaddr.sa_data);
		return 0;
	}

	Genode::warning("ioctl: request ", request, " not handled by switch");
	return -1;
}


int linux_set_iface_flags(int sock, const char *ifname, int dev_up)
{
	return 0;
}


int linux_iface_up(int sock, const char *ifname)
{
	/* in our case the interface is by definition always up */
	return 1;
}


int linux_get_ifhwaddr(int sock, const char *ifname, unsigned char *addr)
{
	socket_call.get_mac_address(addr);
	return 0;
}


int linux_set_ifhwaddr(int sock, const char *ifname, const unsigned char *addr)
{
	return -1;
}


int linux_br_add(int sock, const char *brname) { return -1; }


int linux_br_del(int sock, const char *brname) { return -1; }


int linux_br_add_if(int sock, const char *brname, const char *ifname) {
	return -1; }


int linux_br_del_if(int sock, const char *brname, const char *ifname) {
	return -1; }


int linux_br_get(char *brname, const char *ifname) { return -1; }


int linux_master_get(char *master_ifname, const char *ifname) {
	return -1; }

} /* extern "C" */
