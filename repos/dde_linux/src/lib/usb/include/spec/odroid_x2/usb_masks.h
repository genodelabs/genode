/*
 * \brief  USB registers masks for Odroid-x2
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopez Leon <humberto@uclv.cu>
 * \author Reinir Millo Sanchez <rmillo@uclv.cu>
 * \date   2015-07-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _USB_MASKS_H_
#define _USB_MASKS_H_

enum {
	PHY0_NORMAL_MASK                     = 0x39 << 0,
	PHY0_SWRST_MASK                      = 0x7 << 0,
	PHY1_STD_NORMAL_MASK                 = 0x7 << 6,
	EXYNOS4X12_HSIC0_NORMAL_MASK         = 0x7 << 9,
	EXYNOS4X12_HSIC1_NORMAL_MASK         = 0x7 << 12,
	EXYNOS4X12_HOST_LINK_PORT_SWRST_MASK = 0xf << 7,
	EXYNOS4X12_PHY1_SWRST_MASK           = 0xf << 3,
};

#endif /* _USB_MASKS_H_ */
