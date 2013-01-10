/*
 * \brief  Bounding box collects different refresh ops and combines them.
 * \author Norman Feske
 * \date   2009-11-03
 *
 * taken from Xvfb display application for Nitpicker
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _APP__XEV_TRACK__BOUNDING_BOX_H_
#define _APP__XEV_TRACK__BOUNDING_BOX_H_

/* Genode includes */
#include <util/misc_math.h>

class Bounding_box
{
	private:

		int _x1, _y1, _x2, _y2;

	public:

		/**
		 * Constructor
		 */
		Bounding_box() { reset(); }

		bool valid() { return _x1 <= _x2 && _y1 <= _y2; }

		void reset() { _x1 = 0, _y1 = 0, _x2 = -1, _y2 = -1; }

		void extend(int x, int y, int w, int h)
		{
			int nx2 = x + w - 1;
			int ny2 = y + h - 1;

			if (!valid()) {
				_x1 = x, _y1 = y, _x2 = x + w - 1, _y2 = y + h - 1;
			} else {
				_x1 = Genode::min(_x1, x);
				_y1 = Genode::min(_y1, y);
				_x2 = Genode::max(_x2, nx2);
				_y2 = Genode::max(_y2, ny2);
			}
		}

		int x() { return _x1; }
		int y() { return _y1; }
		int w() { return _x2 - _x1 + 1; }
		int h() { return _y2 - _y1 + 1; }
};

#endif //_APP__XEV_TRACK__BOUNDING_BOX_H_
