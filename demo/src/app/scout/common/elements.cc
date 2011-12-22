/*
 * \brief   Document structure elements
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "miscmath.h"
#include "elements.h"
#include "browser.h"


/*************
 ** Element **
 *************/

Element::~Element()
{
	if (_parent)
		_parent->forget(this);
}


void Element::redraw_area(int x, int y, int w, int h)
{
	x += _x;
	y += _y;

	/* intersect specified area with element geometry */
	int x1 = max(x, _x);
	int y1 = max(y, _y);
	int x2 = min(x + w - 1, _x + _w - 1);
	int y2 = min(y + h - 1, _y + _h - 1);

	if (x1 > x2 || y1 > y2) return;

	/* propagate redraw request to the parent */
	if (_parent)
		_parent->redraw_area(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}


Element *Element::find(int x, int y)
{
	if (x >= _x && x < _x + _w
	 && y >= _y && y < _y + _h
	 && _flags.findable)
		return this;

	return 0;
}


Element *Element::find_by_y(int y)
{
	return (y >= _y && y < _y + _h) ? this : 0;
}


int Element::abs_x() { return _x + (_parent ? _parent->abs_x() : 0); }
int Element::abs_y() { return _y + (_parent ? _parent->abs_y() : 0); }


Element *Element::chapter()
{
	if (_flags.chapter) return this;
	return _parent ? _parent->chapter() : 0;
}


Browser *Element::browser()
{
	return _parent ? _parent->browser() : 0;
}


/********************
 ** Parent element **
 ********************/

void Parent_element::append(Element *e)
{
	if (_last)
		_last->next = e;
	else
		_first = e;

	_last = e;

	e->parent(this);
}


void Parent_element::remove(Element *e)
{
	if (e == _first)
		_first = e->next;

	else {

		/* search specified element in the list */
		Element *ce = _first;
		while (ce->next && (ce->next != e))
			ce = ce->next;

		/* element is not member of the list */
		if (!ce->next) return;

		/* e->_next is the element to remove, skip it in list */
		ce->next = ce->next->next;
	}

	e->next = 0;

	/* update information about last element */
	for (Element *ce = _first; ce; ce = ce->next)
		_last = ce;
}


void Parent_element::forget(Element *e)
{
	if (e->parent() == this)
		remove(e);

	_parent->forget(e);
}


int Parent_element::_format_children(int x, int w)
{
	int y = 0;

	if (w <= 0) return 0;

	for (Element *e = _first; e; e = e->next) {
		e->format_fixed_width(w);
		e->geometry(x, y, e->min_w(), e->min_h());
		y += e->min_h();
	}

	return y;
}


void Parent_element::draw(Canvas *c, int x, int y)
{
	for (Element *e = _first; e; e = e->next)
		e->try_draw(c, _x + x, _y + y);
}


Element *Parent_element::find(int x, int y)
{
	/* check if position is outside the parent element */
	if (x < _x || x >= _x + _w
	 || y < _y || y >= _y + _h)
		return 0;

	x -= _x;
	y -= _y;

	/* check children */
	Element *ret = this;
	for (Element *e = _first; e; e = e->next) {
		Element *res  = e->find(x, y);
		if (res) ret = res;
	}

	return ret;
}


Element *Parent_element::find_by_y(int y)
{
	/* check if position is outside the parent element */
	if (y < _y || y >= _y + _h)
		return 0;

	y -= _y;

	/* check children */
	for (Element *e = _first; e; e = e->next) {
		Element *res  = e->find_by_y(y);
		if (res) return res;
	}

	return this;
}


void Parent_element::geometry(int x, int y, int w, int h)
{
	::Element::geometry(x, y, w, h);

	if (!_last || !_last->is_bottom()) return;

	_last->geometry(_last->x(), h - _last->h(), _last->w(), _last->h());
}


void Parent_element::fill_cache(Canvas *c)
{
	for (Element *e = _first; e; e = e->next) e->fill_cache(c);
}


void Parent_element::flush_cache(Canvas *c)
{
	for (Element *e = _first; e; e = e->next) e->flush_cache(c);
}


void Parent_element::curr_link_destination(Element *dst)
{
	for (Element *e = _first; e; e = e->next) e->curr_link_destination(dst);
}


/***********
 ** Token **
 ***********/

Token::Token(Style *style, const char *str, int len)
{
	_str               = str;
	_len               = len;
	_style             = style;
	_flags.takes_focus = 0;
	_col               = _style ? _style->color : Color(0, 0, 0);
	_outline           = Color(0, 0, 0, 0);

	if (!_style) return;
	_min_w = _style->font->str_w(str, len) + _style->font->str_w(" ", 1);
	_min_h = _style->font->str_h(str, len);
}


void Token::draw(Canvas *c, int x, int y)
{
	if (!_style) return;
	if (_style->attr & Style::ATTR_BOLD)
		_outline.rgba(_col.r, _col.g, _col.b, 32);

	x++; y++;

	if (_outline.a)
		for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++)
			c->draw_string(_x + x +i , _y + y +j, _style->font, _outline, _str, _len);

	c->draw_string(_x + x, _y + y, _style->font, _col, _str, _len);

	if (_flags.link)
		c->draw_box(_x + x, _y + y + _h - 1, _w, 1, Color(0,0,255));
}


/***********
 ** Block **
 ***********/

void Block::append_text(const char *str, Style *style,
                        Text_type type, Anchor *a, Launcher *l)
{
	while (*str) {

		/* skip spaces */
		if (*str == ' ') {
			str++;
			continue;
		}

		/* search end of word */
		int i;
		for (i = 0; str[i] && (str[i] != ' '); i++);

		/* create and append token for the word */
		if (i) {

			if ((type == LAUNCHER) && l)
				append(new Launcher_link_token(style, str, i, l));

			else if ((type == LINK) && a)
				append(new Link_token(style, str, i, a));

			else
				append(new Token(style, str, i));
		}

		/* continue with next word */
		str += i;
	}
}


void Block::format_fixed_width(int w)
{
	int x = 0, y = 0;
	int line_max_h = 0;
	int max_w = 0;

	for (Element *e = _first; e; e = e->next) {

		/* wrap at the end of the line */
		if (x + e->min_w() >= w) {
			x  = _second_indent;
			y += line_max_h;
			line_max_h = 0;
		}

		/* position element */
		if (max_w < x + e->min_w())
			max_w = x + e->min_w();

		e->geometry(x, y, e->min_w(), e->min_h());

		/* determine token with the biggest height of the line */
		if (line_max_h < e->min_h())
			line_max_h = e->min_h();

		x += e->min_w();
	}

	/*
	 * Now, the text is left-aligned.
	 * Let's apply another alignment if specified.
	 */

	if (_align != LEFT) {
		for (Element *line = _first; line; ) {

			Element *e;
			int      cy = line->y();   /* y position of current line */
			int      max_x;            /* rightmost position         */

			/* determine free space at the end of the line */
			for (max_x = 0, e = line; e && (e->y() == cy); e = e->next)
				max_x = max(max_x, e->x() + e->w() - 1);

			/* indent elements of the line according to the alignment */
			int dx = 0;
			if (_align == CENTER) dx = max(0, (max_w - max_x)/2);
			if (_align == RIGHT)  dx = max(0, max_w - max_x);
			for (e = line; e && (e->y() == cy); e = e->next)
				e->geometry(e->x() + dx, e->y(), e->w(), e->h());

			/* find first element of next line */
			for (; line && (line->y() == cy); line = line->next);
		}
	}

	/* line break at the end of the last line */
	if (line_max_h) y += line_max_h;

	_min_h = y + 5;
	_min_w = max_w;
}


/************
 ** Center **
 ************/

void Center::format_fixed_width(int w)
{
	_min_h = _format_children(0, w);

	/* determine highest min with of children */
	int highest_min_w = 0;
	for (Element *e = _first; e; e = e->next)
		if (highest_min_w < e->min_w())
			highest_min_w = e->min_w();

	int dx = (w - highest_min_w)>>1;
	_min_w = max(w, highest_min_w);

	/* move children to center */
	for (Element *e = _first; e; e = e->next)
		e->geometry(dx, e->y(), e->w(), e->h());
}


/**************
 ** Verbatim **
 **************/

void Verbatim::draw(Canvas *c, int x, int y)
{
	static const int pad = 5;

	c->draw_box(_x + x + pad, _y + y + pad, _w - 2*pad, _h - 2*pad, bgcol);

	int cx1 = c->clip_x1(), cy1 = c->clip_y1();
	int cx2 = c->clip_x2(), cy2 = c->clip_y2();

	c->clip(_x + x + pad, _y + y + pad, _w - 2*pad, _h - 2*pad);
	Parent_element::draw(c, x, y);
	c->clip(cx1, cy1, cx2 - cx1 + 1, cy2 - cy1 + 1);
}


void Verbatim::format_fixed_width(int w)
{
	int y = 10;

	for (Element *e = _first; e; e = e->next) {

		/* position element */
		e->geometry(10, y, e->min_w(), e->min_h());

		y += e->min_h();
	}

	_min_h = y + 10;
	_min_w = w;
}


/****************
 ** Link_token **
 ****************/

void Link_token::handle(Event &e)
{
	if (e.type != Event::PRESS) return;

	/* make browser to follow link */
	Browser *b = browser();
	if (b && _dst) b->go_to(_dst);
}


/*************************
 ** Launcher_link_token **
 *************************/

void Launcher_link_token::handle(Event &e)
{
	if (e.type != Event::PRESS) return;

	step(8);
	curr(255);

	/* launch executable */
	Launcher *l = (Launcher *)_dst;
	if (l) l->launch();
}
