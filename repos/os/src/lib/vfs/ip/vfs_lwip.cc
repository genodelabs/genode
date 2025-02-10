#include "vfs_ip.h"

Vfs::Ip_string const &Vfs::ip_stack()
{
	static Ip_string string { "lwip" };
	return string;
}
