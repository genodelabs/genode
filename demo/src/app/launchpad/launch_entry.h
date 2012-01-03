/*
 * \brief  Launcher entry widget
 * \author Norman Feske
 * \date   2006-09-13
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LAUNCH_ENTRY_H_
#define _LAUNCH_ENTRY_H_

#include "loadbar.h"
#include "launcher_config.h"

template <typename PT>
class Launch_entry : public Parent_element, public Loadbar_listener
{
	private:

		Block             _block;
		Kbyte_loadbar<PT> _loadbar;
		Launcher_config   _config;
		Launcher          _launcher;
		int               _lh;        /* launch entry height */

		enum { _PTW = 100 };  /* program text width */
		enum { _PADX = 10 };  /* program text width */
		enum { _PADR = 16 };  /* right padding      */

	public:

		/**
		 * Constructor
		 */
		Launch_entry(const char *prg_name, int initial_quota, int max_quota,
		             Launchpad  *launchpad,
		             Genode::Dataspace_capability config_ds)
		: _block(Block::RIGHT), _loadbar(this, &label_font), _config(config_ds),
		  _launcher(prg_name, launchpad, 1024 * initial_quota, &_config)
		{
			_block.append_launchertext(prg_name, &link_style, &_launcher);

			_loadbar.max_value(max_quota);
			_loadbar.value(initial_quota);
			append(&_loadbar);
			append(&_block);
			_min_w = _PTW + 100;
		}


		/********************************
		 ** Loadbar listener interface **
		 ********************************/

		void loadbar_changed(int mx)
		{
			int value = _loadbar.value_by_xpos(mx - _loadbar.abs_x());
			_loadbar.value(value);
			_loadbar.refresh();
			_launcher.quota(1024 * (unsigned long)value);
		}


		/******************************
		 ** Parent element interface **
		 ******************************/

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
};

#endif
