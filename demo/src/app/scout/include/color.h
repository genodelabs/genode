/*
 * \brief   Color representation
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _COLOR_H_
#define _COLOR_H_


class Color
{
	public:

		enum {
			TRANSPARENT = 0,
			OPAQUE      = 255
		};

		int r, g, b, a;

		/**
		 * Constructors
		 */
		Color(int red, int green, int blue, int alpha = 255)
		{
			rgba(red, green, blue, alpha);
		}

		Color() { rgba(0, 0, 0); }

		/**
		 * Convenience function: Assign rgba values
		 */
		inline void rgba(int red, int green, int blue, int alpha = 255)
		{
			r = red;
			g = green;
			b = blue;
			a = alpha;
		}
};


#endif /* _COLOR_H_ */
