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

/*****************
 ** linux/gfp.h **
 *****************/

enum {
	__GFP_DMA         = 0x00000001u,
	__GFP_HIGHMEM     = 0x00000002u,
	__GFP_DMA32       = 0x00000004u,
	__GFP_MOVABLE     = 0x00000008u,
	__GFP_RECLAIMABLE = 0x00000010u,
	__GFP_HIGH        = 0x00000020u,
	__GFP_IO          = 0x00000040u,
	__GFP_FS          = 0x00000080u,
	__GFP_NOWARN      = 0x00000200u,
	__GFP_RETRY_MAYFAIL = 0x00000400u,
	__GFP_NOFAIL      = 0x00000800u,
	__GFP_NORETRY     = 0x00001000u,
	__GFP_MEMALLOC    = 0x00002000u,
	__GFP_COMP        = 0x00004000u,
	__GFP_ZERO        = 0x00008000u,
	__GFP_NOMEMALLOC  = 0x00010000u,
	__GFP_HARDWALL    = 0x00020000u,
	__GFP_THISNODE    = 0x00040000u,
	__GFP_ATOMIC      = 0x00080000u,
	__GFP_ACCOUNT     = 0x00100000u,
	__GFP_DIRECT_RECLAIM = 0x00200000u,
	__GFP_WRITE          = 0x00800000u,
	__GFP_KSWAPD_RECLAIM = 0x01000000u,

	GFP_LX_DMA           = 0x80000000u,
	__GFP_RECLAIM        = __GFP_DIRECT_RECLAIM | __GFP_KSWAPD_RECLAIM,

	GFP_ATOMIC           = (__GFP_HIGH|__GFP_ATOMIC|__GFP_KSWAPD_RECLAIM),
	GFP_DMA           = __GFP_DMA,
	GFP_DMA32         = __GFP_DMA32,
	GFP_KERNEL        = __GFP_RECLAIM | __GFP_IO | __GFP_FS,
/*
	GFP_TEMPORARY     = __GFP_RECLAIM | __GFP_IO | __GFP_FS | __GFP_RECLAIMABLE,
*/
	GFP_USER          = __GFP_RECLAIM | __GFP_IO | __GFP_FS | __GFP_HARDWALL,
	GFP_HIGHUSER      = GFP_USER | __GFP_HIGHMEM,
};

#define GFP_NOWAIT (__GFP_KSWAPD_RECLAIM)
