/*
 * \brief  Interface for font styles
 * \author Norman Feske
 * \date   2013-01-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__FONT_FACE_H_
#define _TERMINAL__FONT_FACE_H_

class Font_face
{
	public:

		enum Type { REGULAR, ITALIC, BOLD, BOLD_ITALIC };

	private:

		Type _type;

	public:

		Font_face(Type type) : _type(type) { }

		static unsigned char attr_mask() { return 3; }

		unsigned char attr_bits() const { return _type & attr_mask(); }
};

#endif /* _TERMINAL__FONT_FACE_H_ */
