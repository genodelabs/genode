/*
 * \brief   Launchpad window interface
 * \date    2006-08-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LAUNCHPAD_WINDOW_H_
#define _LAUNCHPAD_WINDOW_H_

#include <base/env.h>
#include <dataspace/capability.h>

#include <scout/platform.h>
#include <scout/window.h>

#include "elements.h"
#include "widgets.h"
#include "sky_texture.h"
#include "scrollbar.h"
#include "fade_icon.h"
#include "titlebar.h"

#include "launch_entry.h"
#include "status_entry.h"
#include "child_entry.h"
#include "section.h"

#include <launchpad/launchpad.h>
#include <base/printf.h>

template <typename PT>
class Launchpad_window : public Scout::Scrollbar_listener,
                         public Launchpad,
                         public Scout::Window
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
		Scout::Titlebar<PT>              _titlebar;
		Scout::Sky_texture<PT, 512, 512> _texture;
		Scout::Fade_icon<PT, 32, 32>     _sizer;
		Scout::Scrollbar<PT>             _scrollbar;
		Genode::List<Child_entry<PT> >   _child_entry_list;
		Scout::Docview                   _docview;
		Scout::Spacer                    _spacer;
		Scout::Document                  _document;

		Section<PT>                      _info_section;
		Section<PT>                      _launch_section;
		Section<PT>                      _kiddy_section;

		Status_entry<PT>                 _status_entry;

	public:

		/**
		 * Constructor
		 *
		 * \param initial_quota  maximum value of quota displays
		 */
		Launchpad_window(Genode::Env &env,
		                 Scout::Graphics_backend &gfx_backend,
		                 Scout::Point position, Scout::Area size,
		                 Scout::Area max_size, unsigned long inital_quota);

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
		void format(Scout::Area);
		void ypos(int ypos) { ypos_sb(ypos, 1); }

		/**
		 * Element interface
		 */
		void draw(Scout::Canvas_base &canvas, Scout::Point abs_position)
		{
			using namespace Scout;

			Parent_element::draw(canvas, abs_position);

			/* border */
			Color color(0, 0, 0);
			canvas.draw_box(0, 0, _size.w(), 1, color);
			canvas.draw_box(0, _size.h() - 1, _size.w(), 1, color);
			canvas.draw_box(0, 1, 1, _size.h() - 2, color);
			canvas.draw_box(_size.w() - 1, 1, 1, _size.h() - 2, color);
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

		void add_launcher(Launchpad_child::Name const &name,
		                  unsigned long default_quota,
		                  Genode::Dataspace_capability config_ds = Genode::Dataspace_capability()) override
		{
			Launch_entry<PT> *le;
			le = new Launch_entry<PT>(name, default_quota / 1024,
			                          initial_quota() / 1024,
			                          this, config_ds);
			_launch_section.append(le);
			refresh();
		}

		void add_child(Launchpad_child::Name const &name,
		               unsigned long quota,
		               Launchpad_child &launchpad_child,
		               Genode::Allocator &alloc) override
		{
			Child_entry<PT> *ce;
			ce = new (alloc) Child_entry<PT>(name, quota / 1024,
			                                 initial_quota() / 1024,
			                                 *this, launchpad_child);
			_child_entry_list.insert(ce);
			_kiddy_section.append(ce);
			format(_size);
			refresh();
		}

		void remove_child(Launchpad_child::Name const &name,
		                  Genode::Allocator &alloc) override
		{
			/* lookup child entry by its name */
			Child_entry<PT> *ce = _child_entry_list.first();
			for ( ; ce; ce = ce->Genode::List<Child_entry<PT> >::Element::next())
				if (name == ce->name())
					break;

			if (!ce) {
				PWRN("child entry lookup failed");
				return;
			}

			_child_entry_list.remove(ce);
			_kiddy_section.forget(ce);
			destroy(alloc, ce);
			format(_size);
			refresh();
		}
};

#endif
