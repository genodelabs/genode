/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*******************************
 ** linux/errno.h and friends **
 *******************************/

/**
 * Error codes
 *
 * Note that the codes do not correspond to those of the Linux kernel.
 */
enum {
	/*
	 * The following numbers correspond to FreeBSD
	 */
	EPERM           = 1,
	ENOENT          = 2,
	ESRCH           = 3,
	EINTR           = 4,
	EIO             = 5,
	ENXIO           = 6,
	E2BIG           = 7,
	ENOEXEC         = 8,
	EBADF           = 9,
	EDEADLK         = 11,
	ENOMEM          = 12,
	EACCES          = 13,
	EFAULT          = 14,
	EBUSY           = 16,
	EEXIST          = 17,
	EXDEV           = 18,
	ENODEV          = 19,
	EINVAL          = 22,
	ENFILE          = 23,
	ENOTTY          = 25,
	EFBIG           = 27,
	ENOSPC          = 28,
	ESPIPE          = 29,
	EPIPE           = 32,
	EDOM            = 33,
	ERANGE          = 34,
	EAGAIN          = 35,
	EINPROGRESS     = 36,
	EALREADY        = 37,
	ENOTSOCK        = 38,
	EDESTADDRREQ    = 39,
	EMSGSIZE        = 40,
	ENOPROTOOPT     = 42,
	EPROTONOSUPPORT = 43,
	ESOCKTNOSUPPORT = 44,
	EOPNOTSUPP      = 45,
	EPFNOSUPPORT    = 46,
	EAFNOSUPPORT    = 47,
	EADDRINUSE      = 48,
	EADDRNOTAVAIL   = 49,
	ENETDOWN        = 50,
	ENETUNREACH     = 51,
	ECONNABORTED    = 53,
	ECONNRESET      = 54,
	ENOBUFS         = 55,
	EISCONN         = 56,
	ENOTCONN        = 57,
	ETIMEDOUT       = 60,
	ECONNREFUSED    = 61,
	ENAMETOOLONG    = 63,
	EHOSTDOWN       = 64,
	EHOSTUNREACH    = 65,
	ENOSYS          = 78,
	ENOMSG          = 83,
	EOVERFLOW       = 84,
	ECANCELED       = 85,
	EILSEQ          = 86,
	EBADMSG         = 89,
	ENOLINK         = 91,
	EPROTO          = 92,

	/*
	 * The following numbers correspond to nothing
	 */
	EREMOTEIO       = 200,
	ERESTARTSYS     = 201,
	ENODATA         = 202,
	ETOOSMALL       = 203,
	ENOIOCTLCMD     = 204,
	ENONET          = 205,
	ENOTSUPP        = 206,
	ENOTUNIQ        = 207,
	ERFKILL         = 208,
	ETIME           = 209,
	EPROBE_DEFER    = 210,

	EL3RST          = 211,
	ENOKEY          = 212,
	ECHRNG          = 213,

	MAX_ERRNO       = 4095,
};


 /*****************
  ** linux/err.h **
  *****************/

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)(0UL-MAX_ERRNO))

static inline bool IS_ERR(void const *ptr) {
	return (unsigned long)(ptr) >= (unsigned long)(0UL-MAX_ERRNO); }

static inline void * ERR_PTR(long error) {
	return (void *) error; }

static inline void * ERR_CAST(const void *ptr) {
	return (void *) ptr; }

static inline long IS_ERR_OR_NULL(const void *ptr) {
	return !ptr || IS_ERR_VALUE((unsigned long)ptr); }

static inline long PTR_ERR(const void *ptr) { return (long) ptr; }

