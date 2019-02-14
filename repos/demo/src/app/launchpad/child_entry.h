/*
 * \brief  Child entry widget
 * \author Norman Feske
 * \date   2006-09-13
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_ENTRY_H_
#define _CHILD_ENTRY_H_

#include <util/string.h>
#include <util/list.h>

#include <launchpad/launchpad.h>
#include "loadbar.h"

#define KILL_ICON_RGBA   _binary_kill_icon_rgba_start
#define OPENED_ICON_RGBA _binary_opened_icon_rgba_start
#define CLOSED_ICON_RGBA _binary_closed_icon_rgba_start

extern unsigned char KILL_ICON_RGBA[];
extern unsigned char OPENED_ICON_RGBA[];
extern unsigned char CLOSED_ICON_RGBA[];


class Kill_event_handler : public Scout::Event_handler
{
	private:

		Launchpad       &_launchpad;
		Launchpad_child &_launchpad_child;

	public:

		Kill_event_handler(Launchpad &launchpad, Launchpad_child &launchpad_child):
			_launchpad(launchpad), _launchpad_child(launchpad_child) { }

		/**
		 * Event handler interface
		 */
		void handle_event(Scout::Event const &ev) override
		{
			static int key_cnt;

			using Scout::Event;

			if (ev.type == Event::PRESS)   key_cnt++;
			if (ev.type == Event::RELEASE) key_cnt--;

			if (ev.type == Event::RELEASE && key_cnt == 0)
				_launchpad.exit_child(_launchpad_child);
		}
};


template <typename PT>
class Child_entry : public  Scout::Parent_element,
                    private Genode::List<Child_entry<PT> >::Element
{
	private:

		friend class Genode::List<Child_entry<PT> >;

		enum { _IW       = 16 };      /* icon width               */
		enum { _IH       = 16 };      /* icon height              */
		enum { _PTW      = 100 };     /* program text width       */
		enum { _PADX     = 10 };      /* horizontal padding       */
		enum { _NAME_LEN = 64 };      /* max length of child name */

		Scout::Block      _block;
		Kbyte_loadbar<PT> _loadbar;

		Launchpad_child::Name const _name;

		Scout::Fade_icon<PT, _IW, _IH> _kill_icon { };
		Scout::Fade_icon<PT, _IW, _IH> _fold_icon { };

		Kill_event_handler _kill_event_handler;

	public:

		/**
		 * Constructor
		 */
		Child_entry(Launchpad_child::Name const &name, int quota_kb, int max_quota_kb,
		            Launchpad &launchpad, Launchpad_child &launchpad_child)
		:
			_block(Scout::Block::RIGHT), _loadbar(0, &Scout::label_font),
			_name(name),
			_kill_event_handler(launchpad, launchpad_child)
		{
			_block.append_plaintext(_name.string(), &Scout::plain_style);

			_loadbar.max_value(max_quota_kb);
			_loadbar.value(quota_kb);

			_kill_icon.rgba(KILL_ICON_RGBA, 0, 0);
			_kill_icon.alpha(100);
			_kill_icon.focus_alpha(200);
			_kill_icon.event_handler(&_kill_event_handler);

			_fold_icon.rgba(CLOSED_ICON_RGBA, 0, 0);
			_fold_icon.alpha(100);
			_fold_icon.focus_alpha(200);

			append(&_loadbar);
			append(&_block);
			append(&_kill_icon);
			append(&_fold_icon);

			_min_size = Scout::Area(_PTW + 100, _min_size.h());
		}

        using Genode::List<Child_entry<PT> >::Element::next;

		/**
		 * Accessors
		 */
		Launchpad_child::Name name() { return _name; }


		/******************************
		 ** Parent element interface **
		 ******************************/

		void format_fixed_width(int w) override
		{
			using namespace Scout;

			_block.format_fixed_width(_PTW);
			int bh = _block.min_size().h();
			int iy = max(0U, (bh - _loadbar.min_size().h())/2);

			_fold_icon.geometry(Rect(Point(0, iy), Area(_IW, _IH)));
			_kill_icon.geometry(Rect(Point(w - _IW - 8, iy), Area(_IW, _IH)));

			_block.geometry(Rect(Point(max(10, _PTW - (int)_block.min_size().w()),
			                           max(0, (bh - (int)_block.min_size().h())/2)),
			                     Area(min((int)_PTW,
			                              (int)_block.min_size().w()), bh)));

			int lw = w - 2*_PADX - _PTW - _IW;
			_loadbar.format_fixed_width(lw);
			_loadbar.geometry(Rect(Point(_PADX + _PTW, iy), Area(lw, 16)));

			_min_size = Scout::Area(w, bh);
		}
};

#endif
