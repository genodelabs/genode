/*
 * \brief  Status entry widget
 * \author Norman Feske
 * \date   2006-09-13
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STATUS_ENTRY_H_
#define _STATUS_ENTRY_H_

#include "loadbar.h"

template <typename PT>
class Status_entry : public Scout::Parent_element
{
	private:

		Scout::Block      _block;
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
		: _block(Scout::Block::RIGHT), _loadbar(0, &Scout::label_font)
		{
			_block.append_plaintext(label, &Scout::plain_style);

			_loadbar.max_value(20*1024);
			_loadbar.value(3*1024);

			append(&_loadbar);
			append(&_block);

			_min_size = Scout::Area(_PTW + 100, _min_size.h());
		}

		void format_fixed_width(int w)
		{
			using namespace Scout;

			_block.format_fixed_width(_PTW);
			_lh = _block.min_size().h();
			_block.geometry(Rect(Point(max(10U, _PTW - _block.min_size().w()),
			                           max(0U, (_lh - _block.min_size().h())/2)),
			                     Area(min((unsigned)_PTW, _block.min_size().w()), _lh)));

			int lw = max(0, w - 2*_PADX - _PTW - _PADR);
			int ly = max(0U, (_lh - _loadbar.min_size().h())/2);
			_loadbar.format_fixed_width(lw);
			_loadbar.geometry(Rect(Point(_PADX + _PTW, ly), Area(lw, 16)));

			_min_size = Scout::Area(w, _lh);
		}

		void value(int value) { _loadbar.value(value); }
		void max_value(int max_value) { _loadbar.max_value(max_value); }
};

#endif
