/*
 * \brief  Emulation of the Linux userland API
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2014-07-25
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux headers.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LIBNL_EMUL_H_
#define _LIBNL_EMUL_H_


/*********************
 ** asm/byteorder.h **
 *********************/

#define __LITTLE_ENDIAN_BITFIELD 1


/*******************
 ** linux/types.h **
 *******************/

#include <linux/types.h>


/**********************
 ** uapi/linux/in6.h **
 **********************/

struct in6_addr {
	union {
		__u8        u6_addr8[16];
#if __UAPI_DEF_IN6_ADDR_ALT
		__be16      u6_addr16[8];
		__be32      u6_addr32[4];
#endif
	} in6_u;
#define s6_addr         in6_u.u6_addr8
#if __UAPI_DEF_IN6_ADDR_ALT
#define s6_addr16       in6_u.u6_addr16
#define s6_addr32       in6_u.u6_addr32
#endif
};

struct sockaddr_in6 {
	unsigned short int  sin6_family;    /* AF_INET6 */
	__be16              sin6_port;      /* Transport layer port # */
	__be32              sin6_flowinfo;  /* IPv6 flow information */
	struct in6_addr     sin6_addr;      /* IPv6 address */
	__u32               sin6_scope_id;  /* scope id (new in RFC2553) */
};

/*
 * Socket types should be taken from libc
 */
#define _BITS_SOCKADDR_H


/***********************
 ** uapi/asm/socket.h **
 ***********************/

#define SO_PASSCRED 16


/********************
 ** linux/socket.h **
 ********************/

struct ucred {
	__u32 pid;
	__u32 uid;
	__u32 gid;
};

#define AF_NETLINK 16

#define SCM_CREDENTIALS 0x02

#endif /* _LIBNL_EMUL_H_ */
