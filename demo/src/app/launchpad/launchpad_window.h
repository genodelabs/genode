/*
 * \brief   Launchpad window interface
 * \date    2006-08-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LAUNCHPAD_WINDOW_H_
#define _LAUNCHPAD_WINDOW_H_

#include "elements.h"
#include "widgets.h"
#include "sky_texture.h"
#include "scrollbar.h"
#include "fade_icon.h"
#include "platform.h"
#include "window.h"
#include "titlebar.h"

#include "launch_entry.h"
#include "status_entry.h"
#include "child_entry.h"
#include "section.h"

#include <launchpad/launchpad.h>
#include <base/printf.h>

template <typename PT>
class Launchpad_window : public Scrollbar_listener,
                         public Launchpad,
                         public Window
{
	private:

		/**
		 * Constants
		 */
		enum {
			_TH        = 32,    /* height of title bar    */
			_SB_XPAD   = 5,     /* hor. pad of scrollbar  */
			_SB_YPAD   = 10,    /* vert. pad of scrollbar */
		};

		/**
		 * Widgets
		 */
		Titlebar<PT>                   _titlebar;
		Sky_texture<PT, 512, 512>      _texture;
		Fade_icon<PT, 32, 32>          _sizer;
		Scrollbar<PT>                  _scrollbar;
		Genode::List<Child_entry<PT> > _child_entry_list;
		Docview                        _docview;
		Spacer                         _spacer;
		Document                       _document;

		Section<PT>                    _info_section;
		Section<PT>                    _launch_section;
		Section<PT>                    _kiddy_section;

		Status_entry<PT>               _status_entry;

	public:

		/**
		 * Constructor
		 *
		 * \param initial_quota  maximum value of quota displays
		 */
		Launchpad_window(Platform *pf,
		                 Redraw_manager *redraw, int max_w, int max_h,
		                 unsigned long inital_quota);

		/**
		 * Define vertical scroll offset of document
		 *
		 * \param update_scrollbar  if set to one, adjust scrollbar properties
		 *                          to the new view position.
		 */
		void ypos_sb(int ypos, int update_scrollbar = 1);

		/**
		 * Window interface
		 */
		void format(int w, int h);
		void ypos(int ypos) { ypos_sb(ypos, 1); }

		/**
		 * Element interface
		 */
		void draw(Canvas *c, int x, int y)
		{
			::Parent_element::draw(c, x, y);

			/* border */
			Color col(0, 0, 0);
			c->draw_box(0, 0, _w, 1, col);
			c->draw_box(0, _h - 1, _w, 1, col);
			c->draw_box(0, 1, 1, _h - 2, col);
			c->draw_box(_w - 1, 1, 1, _h - 2, col);
		};

		/**
		 * Scrollbar listener interface
		 */
		void handle_scroll(int view_pos);

		/**
		 * Launchpad interface
		 */
		void quota(unsigned long quota)
		{
			_status_entry.max_value(initial_quota() / 1024);
			_status_entry.value(quota / 1024);
			_status_entry.refresh();
		}

		void add_launcher(const char *filename,
		                  unsigned long default_quota,
		                  Genode::Dataspace_capability config_ds = Genode::Dataspace_capability())
		{
			Launch_entry<PT> *le;
			le = new Launch_entry<PT>(filename, default_quota / 1024,
			                          initial_quota() / 1024,
			                          this, config_ds);
			_launch_section.append(le);
			refresh();
		}

		void add_child(const char *unique_name,
		               unsigned long quota,
		               Launchpad_child *launchpad_child,
		               Genode::Allocator *alloc)
		{
			Child_entry<PT> *ce;
			ce = new (alloc) Child_entry<PT>(unique_name, quota / 1024,
			                                 initial_quota() / 1024,
			                                 this, launchpad_child);
			_child_entry_list.insert(ce);
			_kiddy_section.append(ce);
			format(_w, _h);
			refresh();
		}

		void remove_child(const char *name, Genode::Allocator *alloc)
		{
			/* lookup child entry by its name */
			Child_entry<PT> *ce = _child_entry_list.first();
			for ( ; ce; ce = ce->Genode::List<Child_entry<PT> >::Element::next())
				if (Genode::strcmp(ce->name(), name) == 0)
					break;

			if (!ce) {
				PWRN("child entry lookup failed");
				return;
			}

			_child_entry_list.remove(ce);
			_kiddy_section.forget(ce);
			destroy(alloc, ce);
			format(_w, _h);
			refresh();
		}
};

#endif
