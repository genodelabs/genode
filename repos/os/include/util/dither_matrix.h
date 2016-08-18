/*
 * \brief   Dithering matrix
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__DITHER_MATRIX_H_
#define _INCLUDE__UTIL__DITHER_MATRIX_H_

#include <base/stdint.h>

namespace Genode { class Dither_matrix; }


class Genode::Dither_matrix
{
	private:

		enum { SIZE = 16, MASK = SIZE - 1 };

	public:

		class Row
		{
			private:

				const int *values;

				Row(const int *values) : values(values) { }

				friend class Genode::Dither_matrix;

			public:

				int value(unsigned x) const { return values[x & MASK]; }
		};

		static Row row(unsigned y)
		{
			static const int matrix[SIZE][SIZE] =
			{
			  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
			  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
			  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
			  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
			  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
			  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
			  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
			  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
			  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
			  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
			  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
			  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
			  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
			  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
			  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
			  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
			};

			return Row(matrix[y & MASK]);
		}

		static inline int value(unsigned x, unsigned y)
		{
			return row(y).value(x);
		}
};

#endif /* _INCLUDE__UTIL__DITHER_MATRIX_H_ */
