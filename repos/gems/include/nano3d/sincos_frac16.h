/*
 * \brief  Table of sine and cosine values in 16.16 fractional format
 * \author Norman Feske
 * \date   2015-06-19
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NANO3D__SINCOS_FRAC16_H_
#define _INCLUDE__NANO3D__SINCOS_FRAC16_H_

namespace Nano3d { class Sincos_frac16; };


class Nano3d::Sincos_frac16
{
	public:

		enum { STEPS = 1024 };

	private:

		int _table[STEPS + STEPS/4];

	public:

		inline Sincos_frac16();

		int sin(int angle) const { return _table[angle & (STEPS - 1)]; }
		int cos(int angle) const { return sin(angle + STEPS/4); }
};


Nano3d::Sincos_frac16::Sincos_frac16()
{
	int const cos_mid = 0x7fff;
	int const cos_low = 0x310b; /* cos(360/1024) = 0x7fff6216 */
	int const sin_mid = 0x00c9;
	int const sin_low = 0x07c4; /* sin(360/1024) = 0x00c90f87 */

	int x_mid = 0x7fff;
	int x_low = 0x7fff; /* x = 1.0 */
	int y_mid = 0;
	int y_low = 0;      /* y = 0.0 */

	int nx_high, ny_high;
	int nx_mid,  nx_low;
	int ny_mid,  ny_low;

	for (unsigned i = 0; i < (STEPS >> 2); i++) {

		/* store current sine value */
		_table[i]                    =  y_mid << 1;
		_table[(STEPS >> 1) - i - 1] =  y_mid << 1;
		_table[i + (STEPS >> 1)]     = -y_mid << 1;
		_table[STEPS - i - 1]        = -y_mid << 1;

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
		x_low = (nx_high & 0x80000000) ? (nx_high | (~0U << 16)) : (nx_high & 0xffff);
		x_low = x_low >> 1;
		x_mid = nx_high >> 16;

		y_low = (ny_high & 0x80000000) ? (ny_high | (~0U << 16)) : (ny_high & 0xffff);
		y_low = y_low >> 1;
		y_mid = ny_high >> 16;
	}
}


namespace Nano3d {

	static Sincos_frac16 const &sincos_frac16()
	{
		static Sincos_frac16 inst;
		return inst;
	}


	static inline int sin_frac16(int angle) { return sincos_frac16().sin(angle); }
	static inline int cos_frac16(int angle) { return sincos_frac16().cos(angle); }
};

#endif /* _INCLUDE__NANO3D__SINCOS_FRAC16_H_ */
