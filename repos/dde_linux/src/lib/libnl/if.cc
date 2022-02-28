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
