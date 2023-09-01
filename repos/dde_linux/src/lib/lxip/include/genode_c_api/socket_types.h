/*
 * \brief  Definitions of standard socket API values used when no libc headers
 *         are present
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

enum {
	/* socket domain */
	AF_UNSPEC = 0,
	AF_INET = 2,
	/* socket type */
	SOCK_STREAM = 1,
	SOCK_DGRAM  = 2,
	/* protocol */
	IPPROTO_TCP = 6,
	IPPROTO_UDP = 17,
	/* sockaddr_in */
	INADDR_ANY = 0ul,
	INADDR_BROADCAST = ~0u,

	/* shutdown */
	SHUT_RD = 0,
	SHUT_WR = 1,
	SHUT_RDWR = 2,
};
