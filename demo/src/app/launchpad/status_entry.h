/*
 * \brief  Status entry widget
 * \author Norman Feske
 * \date   2006-09-13
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _STATUS_ENTRY_H_
#define _STATUS_ENTRY_H_

#include "loadbar.h"

template <typename PT>
class Status_entry : public Parent_element
{
	private:

		Block             _block;
		Kbyte_loadbar<PT> _loadbar;
		int               _lh;        /* launch entry height */

		enum { _PTW = 100 };  /* program text width */
		enum { _PADX = 10 };  /* horizontal padding */
		enum { _PADR = 16 };  /* right padding      */

	public:

		/**
		 * Constructor
		 */
		Status_entry(const char *label)
		: _block(Block::RIGHT), _loadbar(0, &label_font)
		{
			_block.append_plaintext(label, &plain_style);

			_loadbar.max_value(20*1024);
			_loadbar.value(3*1024);

			append(&_loadbar);
			append(&_block);

			_min_w = _PTW + 100;
		}

		void format_fixed_width(int w)
		{
			_block.format_fixed_width(_PTW);
			_lh = _block.min_h();
			_block.geometry(max(10, _PTW - _block.min_w()),
			                max(0, (_lh - _block.min_h())/2),
			                min((int)_PTW, _block.min_w()), _lh);

			int lw = max(0, w - 2*_PADX - _PTW - _PADR);
			int ly = max(0, (_lh - _loadbar.min_h())/2);
			_loadbar.format_fixed_width(lw);
			_loadbar.geometry(_PADX + _PTW, ly, lw, 16);
			_min_h = _lh;
			_min_w = w;
		}

		void value(int value) { _loadbar.value(value); }
		void max_value(int max_value) { _loadbar.max_value(max_value); }
};

#endif
