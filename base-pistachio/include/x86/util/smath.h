/*
 * \brief  Simple math calls
 * \author Julian Stecklina
 * \date   2008-02-20
 */

/*
 * Copyright (C) 2008-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

namespace SMath {

	static inline float sinf(float x)
	{
		float res;

		asm ("fsin"
		 : "=t" (res)   /* output   */
		 : "0" (x)      /* input    */
		 :              /* clobbers */
		 );

		return res;
	}

	static inline float cosf(float x)
	{
		float res;

		asm ("fcos"
		 : "=t" (res)   /* output   */
		 : "0" (x)      /* input    */
		 :              /* clobbers */
		 );

		return res;
	}

	static inline float sqrtf(float x)
	{
		float res;

		asm ("fsqrt"
		 : "=t" (res)   /* output   */
		 : "0" (x)      /* input    */
		 :              /* clobbers */
		 );

		return res;
	}
}
