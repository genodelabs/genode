/*
 * \brief  DDE iPXE local helpers
 * \author Christian Helmuth
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#define FMT_BUSDEVFN "%02x:%02x.%x"

#define LOG(fmt, ...)                                           \
	do {                                                        \
		dde_printf("\033[36m" fmt "\033[0m\n", ##__VA_ARGS__ ); \
	} while (0)
