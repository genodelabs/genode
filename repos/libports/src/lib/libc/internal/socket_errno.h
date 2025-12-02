/*
 * \brief  Convert Genode's socket errno to libc errno
 * \author Sebastian Sumpf
 * \date   2025-12-03
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__SOCKET_ERRNO_H_
#define _LIBC__INTERNAL__SOCKET_ERRNO_H_

#include <base/node.h>

namespace Libc {
	int socket_errno(int genode_errno);
}

#endif /* _LIBC__INTERNAL__SOCKET_ERRNO_H_ */
