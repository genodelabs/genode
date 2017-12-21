/*
 * \brief  Launcher entry widget
 * \author Norman Feske
 * \date   2006-09-13
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LAUNCH_ENTRY_H_
#define _LAUNCH_ENTRY_H_

#include "loadbar.h"
#include "launcher_config.h"

template <typename PT>
class Launch_entry : public Scout::Parent_element, public Loadbar_listener
{
	private:

		Scout::Launcher::Name  _prg_name;
		Scout::Block           _block;
		Kbyte_loadbar<PT>      _loadbar;
		Scout::Launcher_config _config;
		Scout::Launcher        _launcher;
		int                    _lh = 0;        /* launch entry height */

		enum { _PTW = 100 };  /* program text width */
		enum { _PADX = 10 };  /* program text width */
		enum { _PADR = 16 };  /* right padding      */

	public:

		/**
		 * Constructor
		 */
		Launch_entry(Scout::Launcher::Name const &prg_name,
		             unsigned long caps, unsigned long initial_quota,
		             unsigned long max_quota, Launchpad  *launchpad,
		             Genode::Dataspace_capability config_ds)
		:
			_prg_name(prg_name),
			_block(Scout::Block::RIGHT), _loadbar(this, &Scout::label_font),
			_config(config_ds),
			_launcher(prg_name, launchpad, caps, initial_quota * 1024UL, &_config)
		{
			_block.append_launchertext(_prg_name.string(), &Scout::link_style, &_launcher);

			_loadbar.max_value(max_quota);
			_loadbar.value(initial_quota);
			append(&_loadbar);
			append(&_block);

			_min_size = Scout::Area(_PTW + 100, _min_size.h());
		}


		/********************************
		 ** Loadbar listener interface **
		 ********************************/

		void loadbar_changed(int mx)
		{
			int value = _loadbar.value_by_xpos(mx - _loadbar.abs_position().x());
			_loadbar.value(value);
			_loadbar.refresh();
			_launcher.quota(1024 * (unsigned long)value);
		}


		/******************************
		 ** Parent element interface **
		 ******************************/

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
};

#endif
