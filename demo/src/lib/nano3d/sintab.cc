/*
 * \brief   Sine/Cosine table generator
 * \author  Norman Feske
 * \date    2004-11-16
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <nano3d/misc_math.h>

namespace Nano3d {

	int sintab[SINCOSTAB_SIZE];
	int costab[SINCOSTAB_SIZE];
}

/**
 * Generate sine/cosine tables
 */
void Nano3d::init_sincos_tab(void)
{
	int cos_mid = 0x7fff;
	int cos_low = 0x310b; /* cos(360/1024) = 0x7fff6216 */
	int sin_mid = 0x00c9;
	int sin_low = 0x07c4; /* sin(360/1024) = 0x00c90f87 */
	int x_mid   = 0x7fff;
	int x_low   = 0x7fff; /* x = 1.0 */
	int y_mid   = 0;
	int y_low   = 0;      /* y = 0.0 */
	int nx_high, ny_high;
	int nx_mid,  nx_low;
	int ny_mid,  ny_low;

	for (int i = 0; i < (SINCOSTAB_SIZE >> 2); i++) {

		/* store current sine value */
		sintab[i]                             =  y_mid << 1;
		sintab[(SINCOSTAB_SIZE >> 1) - i - 1] =  y_mid << 1;
		sintab[i + (SINCOSTAB_SIZE >> 1)]     = -y_mid << 1;
		sintab[SINCOSTAB_SIZE - i - 1]        = -y_mid << 1;

		/* rotate sin/cos values */

		/* x' = x*cos - y*sin */

		nx_low  = x_low*cos_low
		        - y_low*sin_low;

		nx_mid  = x_low*cos_mid + x_mid*cos_low
		        - y_low*sin_mid - y_mid*sin_low;
		nx_mid += (nx_low >> 14);

		nx_high = x_mid*cos_mid
		        - y_mid*sin_mid
		        + (nx_mid >> 15);

		nx_high = nx_high << 1;

		/* y' = y*cos + x*sin */

		ny_low  = y_low*cos_low
		        + x_low*sin_low;

		ny_mid  = y_low*cos_mid + y_mid*cos_low
		        + x_low*sin_mid + x_mid*sin_low
		        + (ny_low >> 14);

		ny_high = y_mid*cos_mid
		        + x_mid*sin_mid
		        + (ny_mid >> 15);

		ny_high = ny_high << 1;

		/* use new sin/cos values for next iteration, preserve sign */
		x_low = (nx_high & 0x80000000) ? (nx_high | (~0 << 16)) : (nx_high & 0xffff);
		x_low = x_low >> 1;
		x_mid = nx_high >> 16;

		y_low = (ny_high & 0x80000000) ? (ny_high | (~0 << 16)) : (ny_high & 0xffff);
		y_low = y_low >> 1;
		y_mid = ny_high >> 16;
	}

	/* copy sine to cosine values */
	for (int i = 0; i < SINCOSTAB_SIZE; i++)
		costab[i] = sintab[(i + (SINCOSTAB_SIZE >> 2)) % SINCOSTAB_SIZE];
}
