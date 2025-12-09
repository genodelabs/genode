#include "vfs_ip.h"

Vfs_ip::Ip_string const &Vfs_ip::ip_stack()
{
	static Ip_string string { "lwip" };
	return string;
}
