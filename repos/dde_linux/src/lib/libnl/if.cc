/*
 * \brief  Interface query functions
 * \author Josef Soentgen
 * \date   2014-11-22
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <sys/socket.h>
#include <net/if.h>

#include <util/string.h>

extern "C" {

unsigned int wifi_ifindex(void);
char const * wifi_ifname(void);

unsigned int if_nametoindex(const char *ifname)
{
	return wifi_ifindex();
}


char *if_indextoname(unsigned int ifindex, char *ifname)
{
	char const *p = wifi_ifname();
	Genode::memcpy(ifname, p, Genode::strlen(p));
	return ifname;
}

} /* extern "C" */
