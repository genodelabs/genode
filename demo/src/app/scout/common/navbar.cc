/*
 * \brief   Document navigation element
 * \date    2005-11-23
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "elements.h"
#include "widgets.h"
#include "canvas_rgb565.h"
#include "styles.h"
#include "browser.h"


/**
 * Configuration
 */
enum {
	ARROW_H = 64,   /* height of arrow gfx */
	ARROW_W = 64,   /* width  of arrow gfx */
};


Generic_icon *Navbar::next_icon;
Generic_icon *Navbar::prev_icon;


class Linkicon_event_handler : public Event_handler
{
	private:

		Anchor *_dst;
		Navbar *_navbar;

	public:

		/**
		 * Constructor
		 */
		Linkicon_event_handler() { _dst = 0; _navbar = 0; }

		/**
		 * Assign link destination
		 */
		void destination(Navbar *navbar, Anchor *dst)
		{
			_dst    = dst;
			_navbar = navbar;
		}

		/**
		 * Event handler interface
		 */
		void handle(Event &ev)
		{
			if (ev.type != Event::PRESS || !_navbar) return;

			Browser *b = _navbar->browser();
			if (!b || !_dst) return;

			_navbar->curr(0);
			b->go_to(_dst);
			_navbar->fade_to(100, 2);
		}
};


static Linkicon_event_handler next_ev_handler;
static Linkicon_event_handler prev_ev_handler;


Navbar::Navbar()
{
	_next_title  = _prev_title  = 0;
	_next_anchor = _prev_anchor = 0;

	_flags.bottom = 1;

	next_ev_handler.destination(0, 0);
	prev_ev_handler.destination(0, 0);
}


void Navbar::next_link(const char *title, Anchor *dst)
{
	_next_title = new Block(Block::RIGHT);
	_next_anchor = dst;
	_next_title->append_plaintext(title, &navbar_style);
	append(_next_title);
	next_ev_handler.destination(0, 0);
}


void Navbar::prev_link(const char *title, Anchor *dst)
{
	_prev_title = new Block(Block::LEFT);
	_prev_anchor = dst;
	_prev_title->append_plaintext(title, &navbar_style);
	append(_prev_title);
	prev_ev_handler.destination(0, 0);
}


void Navbar::format_fixed_width(int w)
{
	const int padx = 10;  /* free space in the center */

	/* format labels */
	int text_w = w/2 - ARROW_W - padx;
	if (_next_title) _next_title->format_fixed_width(text_w);
	if (_prev_title) _prev_title->format_fixed_width(text_w);

	/* determine right-alignment offset for right label */
	int next_dx = _next_title ? text_w - _next_title->min_w() : 0;

	/* determine bounding box of navbar */
	int h = ARROW_H;
	if (_next_title) h = max(h, _next_title->min_h());
	if (_prev_title) h = max(h, _prev_title->min_h());
	h += 16;

	/* assign icons to this navbar instance */
	next_icon->parent(this);
	prev_icon->parent(this);

	next_ev_handler.destination(this, _next_anchor);
	prev_ev_handler.destination(this, _prev_anchor);

	next_icon->event_handler(&next_ev_handler);
	prev_icon->event_handler(&prev_ev_handler);

	/* place icons */
	int ypos = (h - ARROW_H)/2;
	next_icon->geometry(w - 64, ypos, ARROW_W, ARROW_H);
	prev_icon->geometry(0, ypos, ARROW_W, ARROW_H);

	/* place labels */
	if (_next_title) {
		ypos = (h - _next_title->min_h())/2 + 1;
		_next_title->geometry(w/2 + padx + next_dx, ypos, text_w, _next_title->min_h());
	}
	if (_prev_title) {
		ypos = (h - _prev_title->min_h())/2 + 1;
		_prev_title->geometry(ARROW_W, ypos, text_w, _prev_title->min_h());
	}

	_min_w = w;
	_min_h = h;
}


void Navbar::draw(Canvas *c, int x, int y)
{
	int cx1 = c->clip_x1(), cy1 = c->clip_y1();
	int cx2 = c->clip_x2(), cy2 = c->clip_y2();

	/* shrink clipping area to text area (cut too long words) */
	int nx1 = max(cx1, _x + x + ARROW_W);
	int ny1 = max(cy1, _y + y);
	int nx2 = min(cx2, nx1 + _w - 2*ARROW_W);
	int ny2 = min(cy2, ny1 + _h);

	c->clip(nx1, ny1, nx2 - nx1 + 1, ny2 - ny1 + 1);
	Parent_element::draw(c, x, y);
	c->clip(cx1, cy1, cx2 - cx1 + 1, cy2 - cy1 + 1);

	if (_prev_title) prev_icon->draw(c, _x + x, _y + y);
	if (_next_title) next_icon->draw(c, _x + x, _y + y);
}


Element *Navbar::find(int x, int y)
{
	Element *res;

	if (_prev_title && (res = prev_icon->find(x - _x, y - _y))) return res;
	if (_next_title && (res = next_icon->find(x - _x, y - _y))) return res;

	return ::Parent_element::find(x, y);
}


int Navbar::on_tick()
{
	/* call on_tick function of the fader */
	if (::Fader::on_tick() == 0) return 0;

	prev_icon->alpha(_curr_value);
	next_icon->alpha(_curr_value);
	navbar_style.color.rgba(0, 0, 0, _curr_value);

	refresh();
	return 1;
}
