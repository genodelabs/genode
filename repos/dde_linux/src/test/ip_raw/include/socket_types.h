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
	/* sockaddr_in */
	INADDR_ANY       = 0ul,

	/* shutdown */
	SHUT_RDWR = 2,
};
