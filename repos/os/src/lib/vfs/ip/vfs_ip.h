#ifndef _VFS_IP_H_
#define _VFS_IP_H_

#include <util/string.h>

namespace Vfs {

	using Ip_string = Genode::String<5>;

	/* return name of IP stack for VFS-plugin */
	Ip_string const &ip_stack() ;
}

#endif /* _VFS_IP_H_ */
