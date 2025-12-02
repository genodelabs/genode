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

#include <genode_c_api/socket.h>

#include <errno.h>

#include <internal/socket_errno.h>

int Libc::socket_errno(int genode_errno)
{
	/* -1 means "value not present" in our libc */
	static int table[GENODE_MAX_ERRNO] = {
		[GENODE_ENONE]           = 0,
		[GENODE_E2BIG]           = E2BIG,
		[GENODE_EACCES]          = EACCES,
		[GENODE_EADDRINUSE]      = EADDRINUSE,
		[GENODE_EADDRNOTAVAIL]   = EADDRNOTAVAIL,
		[GENODE_EAFNOSUPPORT]    = EAFNOSUPPORT,
		[GENODE_EAGAIN]          = EAGAIN,
		[GENODE_EALREADY]        = EALREADY,
		[GENODE_EBADF]           = EBADF,
		[GENODE_EBADFD]          = -1,
		[GENODE_EBADMSG]         = EBADMSG,
		[GENODE_EBADRQC]         = -1,
		[GENODE_EBUSY]           = EBUSY,
		[GENODE_ECONNABORTED]    = ECONNABORTED,
		[GENODE_ECONNREFUSED]    = ECONNREFUSED,
		[GENODE_EDESTADDRREQ]    = EDESTADDRREQ,
		[GENODE_EDOM]            = EDOM,
		[GENODE_EEXIST]          = EEXIST,
		[GENODE_EFAULT]          = EFAULT,
		[GENODE_EFBIG]           = EFBIG,
		[GENODE_EHOSTUNREACH]    = EHOSTUNREACH,
		[GENODE_EINPROGRESS]     = EINPROGRESS,
		[GENODE_EINTR]           = EINTR,
		[GENODE_EINVAL]          = EINVAL,
		[GENODE_EIO]             = EIO,
		[GENODE_EISCONN]         = EISCONN,
		[GENODE_ELOOP]           = ELOOP,
		[GENODE_EMLINK]          = EMLINK,
		[GENODE_EMSGSIZE]        = EMSGSIZE,
		[GENODE_ENAMETOOLONG]    = ENAMETOOLONG,
		[GENODE_ENETDOWN]        = ENETDOWN,
		[GENODE_ENETUNREACH]     = ENETUNREACH,
		[GENODE_ENFILE]          = ENFILE,
		[GENODE_ENOBUFS]         = ENOBUFS,
		[GENODE_ENODATA]         = -1,
		[GENODE_ENODEV]          = ENODEV,
		[GENODE_ENOENT]          = ENOENT,
		[GENODE_ENOIOCTLCMD]     = -1,
		[GENODE_ENOLINK]         = ENOLINK,
		[GENODE_ENOMEM]          = ENOMEM,
		[GENODE_ENOMSG]          = ENOMSG,
		[GENODE_ENOPROTOOPT]     = ENOPROTOOPT,
		[GENODE_ENOSPC]          = ENOSPC,
		[GENODE_ENOSYS]          = ENOSYS,
		[GENODE_ENOTCONN]        = ENOTCONN,
		[GENODE_ENOTSUPP]        = ENOTSUP,
		[GENODE_ENOTTY]          = ENOTTY,
		[GENODE_ENXIO]           = ENXIO,
		[GENODE_EOPNOTSUPP]      = EOPNOTSUPP,
		[GENODE_EOVERFLOW]       = EOVERFLOW,
		[GENODE_EPERM]           = EPERM,
		[GENODE_EPFNOSUPPORT]    = EPFNOSUPPORT,
		[GENODE_EPIPE]           = EPIPE,
		[GENODE_EPROTO]          = EPROTO,
		[GENODE_EPROTONOSUPPORT] = EPROTONOSUPPORT,
		[GENODE_EPROTOTYPE]      = EPROTOTYPE,
		[GENODE_ERANGE]          = ERANGE,
		[GENODE_EREMCHG]         = -1,
		[GENODE_ESOCKTNOSUPPORT] = ESOCKTNOSUPPORT,
		[GENODE_ESPIPE]          = ESPIPE,
		[GENODE_ESRCH]           = ESRCH,
		[GENODE_ESTALE]          = ESTALE,
		[GENODE_ETIMEDOUT]       = ETIMEDOUT,
		[GENODE_ETOOMANYREFS]    = ETOOMANYREFS,
		[GENODE_EUSERS]          = EUSERS,
		[GENODE_EXDEV]           = EXDEV,
		[GENODE_ECONNRESET]      = ECONNRESET,
	};

	if (genode_errno < 0 || genode_errno >= GENODE_MAX_ERRNO) {
		Genode::error("Unknown genode_error: ", genode_errno);
		return -1;
	}

	int error = table[genode_errno];
	if (error < 0) {
		Genode::error("Unsupported libc error: ", genode_errno);
		return -1;
	}

	return error;
}
