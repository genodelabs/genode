/*
 * \brief  Terminal font handling
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FONT_FAMILY_H_
#define _FONT_FAMILY_H_

/* local includes */
#include "types.h"

namespace Terminal {
	typedef Text_painter::Font Font;
	class Font_family;
}


class Terminal::Font_family
{
	private:

		Font const &_regular;

	public:

		Font_family(Font const &regular) : _regular(regular) { }

		/**
		 * Return font for specified face
		 *
		 * For now, we do not support font faces other than regular.
		 */
		Font const &font(Font_face) const { return _regular; }

		unsigned cell_width()  const { return _regular.bounding_box().w(); }
		unsigned cell_height() const { return _regular.bounding_box().h(); }
};

#endif /* _FONT_FAMILY_H_ */
