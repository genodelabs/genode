/*
 * \brief  VESA constants and definitions
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VESA_H_
#define _VESA_H_

#include <base/stdint.h>

namespace Vesa {

	/**
	 * VESA mode definitions
	 */
	struct vesa_depth
	{
		unsigned long     depth; /* in bit */
		Genode::uint16_t  mode;  /* 16 bit hex value */
	};

	struct vesa_modes
	{
		unsigned long     width;     /* screen width in pixel */
		unsigned long     height;
		int               num_depth; /* number of struct vesa_depth entries */
		struct vesa_depth depth[4];
	};

	/**
	 * VESA constants
	 */
	enum {
		/* VESA functions */
		VBE_CONTROL_FUNC = 0x4F00,
		VBE_INFO_FUNC    = 0x4F01,
		VBE_MODE_FUNC    = 0x4F02,
		VBE_GMODE_FUNC   = 0x4F03,

		/* VESA return codes */
		VBE_SUPPORTED = 0x4F,
		VBE_SUCCESS   = 0x00,

		/* VESA MASKS/FLAGS */
		VBE_CUR_REFRESH_MASK = 0xF7FF,   /* use current refresh rate */
		VBE_SET_FLAT_FB      = 1 << 14,  /* flat frame buffer flag */
	};


	/****************
	 ** VESA modes **
	 ****************/

	static struct vesa_modes mode_table[] = {
		{640 , 480 , 3, {{15, 0x110}, {16, 0x111}, {24, 0x112}}},
		{800 , 600 , 3, {{15, 0x113}, {16, 0x114}, {24, 0x115}}},
		{1024, 768 , 3, {{15, 0x116}, {16, 0x117}, {24, 0x118}}},
		{1280, 1024, 3, {{15, 0x119}, {16, 0x11A}, {24, 0x11B}}},
		{1600, 1200, 3, {{15, 0x11d}, {16, 0x11e}, {24, 0x11f}}},
	};

	enum {
		MODE_COUNT     = sizeof(mode_table)/sizeof(*mode_table),
		VESA_CTRL_OFFS = 0x100,
		VESA_MODE_OFFS = 0x800,
	};

	/**
	 * Find vesa mode, return mode number or 0
	 */
	static inline Genode::uint16_t get_default_vesa_mode(unsigned long width,
	                                                     unsigned long height,
	                                                     unsigned long depth)
	{
		Genode::uint16_t mode = 0;

		for (int i = 0; i < MODE_COUNT; i++) {

			if (mode_table[i].width != width || mode_table[i].height != height)
				continue;

			for (int j = 0; j < mode_table[i].num_depth; j++) {

				if (mode_table[i].depth[j].depth != depth)
					continue;
				mode = mode_table[i].depth[j].mode;
				break;
			}

			if (mode) break;
		}
		return mode;
	}
}

#endif /* _VESA_H */
