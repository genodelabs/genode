/*
 * \brief  Display controller
 * \author Martin Stein
 * \author Norman Feske
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DISPC_H_
#define _DISPC_H_

/* Genode includes */
#include <util/mmio.h>

struct Dispc : Genode::Mmio
{
	/**
	 * Configures the display controller module for outputs LCD 1 and TV
	 */
	struct Control1 : Register<0x40, 32>
	{
		struct Lcd_enable : Bitfield<0, 1> { };
		struct Tv_enable : Bitfield<1, 1> { };
		struct Go_lcd : Bitfield<5, 1>
		{
			enum { HW_UPDATE_DONE    = 0x0,   /* set by HW after updating */
			       REQUEST_HW_UPDATE = 0x1 }; /* must be set by user */
		};
		struct Go_tv : Bitfield<6, 1>
		{
			enum { HW_UPDATE_DONE    = 0x0,   /* set by HW after updating */
			       REQUEST_HW_UPDATE = 0x1 }; /* must be set by user */
		};
	};

	/**
	 * Configures the display controller module for outputs LCD 1 and TV
	 */
	struct Config1 : Register<0x44, 32>
	{
		/**
		 * Loading of palette/gamma table
		 */
		struct Load_mode : Bitfield<1, 2>
		{
			enum { DATA_EVERY_FRAME = 0x2, };
		};
	};

	struct Size_tv : Register<0x78, 32>
	{
		struct Width  : Bitfield<0, 11>  { };
		struct Height : Bitfield<16, 11> { };
	};
	struct Size_lcd : Register<0x7c, 32>
	{
		struct Width  : Bitfield<0, 11>  { };
		struct Height : Bitfield<16, 11> { };
	};

	/**
	 * Configures base address of the graphics buffer
	 */
	struct Gfx_ba0 : Register<0x80, 32> { };
	struct Gfx_ba1 : Register<0x84, 32> { };

	/**
	 * Configures the size of the graphics window
	 */
	struct Gfx_size : Register<0x8c, 32>
	{
		struct Sizex : Bitfield<0,11>  { };
		struct Sizey : Bitfield<16,11> { };
	};

	/**
	 * Configures the graphics attributes
	 */
	struct Gfx_attributes : Register<0xa0, 32>
	{
		struct Enable : Bitfield<0, 1> { };

		struct Format : Bitfield<1, 5>
		{
			enum { RGB16  = 0x6,
			       ARGB32 = 0xc,
			       RGBA32 = 0xd };
		};
		
		/**
		 * Select GFX channel output
		 */
		struct Channelout : Bitfield<8, 1>
		{
			enum { TV = 0x1 };
		};

		struct Channelout2 : Bitfield<30, 2>
		{
			enum { PRIMARY_LCD = 0 };
		};
	};

	struct Global_buffer : Register<0x800, 32> { };

	struct Divisor : Register<0x804, 32>
	{
		struct Enable : Bitfield<0, 1>  { };
		struct Lcd    : Bitfield<16, 8> { };
	};

	/**
	 * Constructor
	 *
	 * \param mmio_base  base address of DISPC MMIO
	 */
	Dispc(Genode::addr_t const mmio_base)
	:
		Mmio(mmio_base)
	{ }
};

#endif /* _DISPC_H_ */
