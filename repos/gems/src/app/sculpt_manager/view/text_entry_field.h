/*
 * \brief  Helper for implementing editable text fields
 * \author Norman Feske
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__TEXT_ENTRY_FIELD_H_
#define _VIEW__TEXT_ENTRY_FIELD_H_

#include <view/dialog.h>


namespace Sculpt {

	template <size_t> struct Text_entry_field;

	enum {
		CODEPOINT_BACKSPACE = 8,      CODEPOINT_NEWLINE  = 10,
		CODEPOINT_UP        = 0xf700, CODEPOINT_DOWN     = 0xf701,
		CODEPOINT_LEFT      = 0xf702, CODEPOINT_RIGHT    = 0xf703,
		CODEPOINT_HOME      = 0xf729, CODEPOINT_INSERT   = 0xf727,
		CODEPOINT_DELETE    = 0xf728, CODEPOINT_END      = 0xf72b,
		CODEPOINT_PAGEUP    = 0xf72c, CODEPOINT_PAGEDOWN = 0xf72d,
	};
}


template <Sculpt::size_t N>
struct Sculpt::Text_entry_field
{
	Codepoint _elements[N] { };

	unsigned cursor_pos = 0;

	static bool _printable(Codepoint c)
	{
		return (c.value >= 32) && (c.value <= 126);
	}

	void apply(Codepoint c)
	{
		if (c.value == CODEPOINT_BACKSPACE && cursor_pos > 0) {
			cursor_pos--;
			_elements[cursor_pos] = { };
		}

		if (_printable(c) && (cursor_pos + 1) < N) {
			_elements[cursor_pos] = c;
			cursor_pos++;
		}
	}

	Text_entry_field(auto const &s)
	{
		for (Utf8_ptr utf8 = s.string(); utf8.complete(); utf8 = utf8.next())
			apply(utf8.codepoint());
	}

	void print(Output &out) const
	{
		for (unsigned i = 0; i < N; i++)
			if (_printable(_elements[i]))
				Genode::print(out, _elements[i]);
	}
};

#endif /* _VIEW__TEXT_ENTRY_FIELD_H_ */
