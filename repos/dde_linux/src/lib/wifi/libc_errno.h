/*
 * \brief  Libc errno values
 * \date   2022-03-03
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WLAN__LIBC_ERRNO_H_
#define _WLAN__LIBC_ERRNO_H_

/* Linux errno values */
#include <uapi/asm-generic/errno.h>

namespace Libc {

	enum class Errno : int {
		/*
		 * The following numbers correspond to the FreeBSD errno values
		 */
		BSD_EPERM           = 1,
		BSD_ENOENT          = 2,
		BSD_ESRCH           = 3,
		BSD_EINTR           = 4,
		BSD_EIO             = 5,
		BSD_ENXIO           = 6,
		BSD_E2BIG           = 7,
		BSD_ENOEXEC         = 8,
		BSD_EBADF           = 9,
		BSD_EDEADLK         = 11,
		BSD_ENOMEM          = 12,
		BSD_EACCES          = 13,
		BSD_EFAULT          = 14,
		BSD_EBUSY           = 16,
		BSD_EEXIST          = 17,
		BSD_EXDEV           = 18,
		BSD_ENODEV          = 19,
		BSD_EINVAL          = 22,
		BSD_ENFILE          = 23,
		BSD_ENOTTY          = 25,
		BSD_EFBIG           = 27,
		BSD_ENOSPC          = 28,
		BSD_ESPIPE          = 29,
		BSD_EPIPE           = 32,
		BSD_EDOM            = 33,
		BSD_ERANGE          = 34,
		BSD_EAGAIN          = 35,
		BSD_EINPROGRESS     = 36,
		BSD_EALREADY        = 37,
		BSD_ENOTSOCK        = 38,
		BSD_EDESTADDRREQ    = 39,
		BSD_EMSGSIZE        = 40,
		BSD_ENOPROTOOPT     = 42,
		BSD_EPROTONOSUPPORT = 43,
		BSD_ESOCKTNOSUPPORT = 44,
		BSD_EOPNOTSUPP      = 45,
		BSD_EPFNOSUPPORT    = 46,
		BSD_EAFNOSUPPORT    = 47,
		BSD_EADDRINUSE      = 48,
		BSD_EADDRNOTAVAIL   = 49,
		BSD_ENETDOWN        = 50,
		BSD_ENETUNREACH     = 51,
		BSD_ECONNABORTED    = 53,
		BSD_ECONNRESET      = 54,
		BSD_ENOBUFS         = 55,
		BSD_EISCONN         = 56,
		BSD_ENOTCONN        = 57,
		BSD_ETIMEDOUT       = 60,
		BSD_ECONNREFUSED    = 61,
		BSD_ENAMETOOLONG    = 63,
		BSD_EHOSTDOWN       = 64,
		BSD_EHOSTUNREACH    = 65,
		BSD_ENOSYS          = 78,
		BSD_ENOMSG          = 83,
		BSD_EOVERFLOW       = 84,
		BSD_ECANCELED       = 85,
		BSD_EILSEQ          = 86,
		BSD_EBADMSG         = 89,
		BSD_ENOLINK         = 91,
		BSD_EPROTO          = 92,

		/*
		 * The following numbers correspond to nothing
		 */
		BSD_EREMOTEIO       = 200,
		BSD_ERESTARTSYS     = 201,
		BSD_ENODATA         = 202,
		BSD_ETOOSMALL       = 203,
		BSD_ENOIOCTLCMD     = 204,
		BSD_ENONET          = 205,
		BSD_ENOTSUPP        = 206,
		BSD_ENOTUNIQ        = 207,
		BSD_ERFKILL         = 208,
		BSD_ETIME           = 209,
		BSD_EPROBE_DEFER    = 210,

		BSD_EL3RST          = 211,
		BSD_ENOKEY          = 212,
		BSD_ECHRNG          = 213,

		MAX_ERRNO       = 4095,
	};
} /* namespace Libc */

#endif /* _WLAN__LIBC_ERRNO_H_ */
