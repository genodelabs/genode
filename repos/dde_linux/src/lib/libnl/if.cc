#include <sys/socket.h>
#include <net/if.h>

#include <util/string.h>

#define WLAN_DEV "wlan0"

extern "C" {

unsigned int if_nametoindex(const char *ifname)
{
	/* we make sure the index is always 1 in the wifi driver */
	return 1;
}


char *if_indextoname(unsigned int ifindex, char *ifname)
{
	Genode::memcpy(ifname, WLAN_DEV, Genode::strlen(WLAN_DEV));
	return ifname;
}

} /* extern "C" */
