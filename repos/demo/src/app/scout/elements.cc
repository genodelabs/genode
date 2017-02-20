/*
 * \brief   Document structure elements
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <scout/misc_math.h>

#include "elements.h"
#include "browser.h"

using namespace Scout;


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
	x += _position.x();
	y += _position.y();

	/* intersect specified area with element geometry */
	int x1 = max(x, _position.x());
	int y1 = max(y, _position.y());
	int x2 = min(x + w - 1, _position.x() + (int)_size.w() - 1);
	int y2 = min(y + h - 1, _position.y() + (int)_size.h() - 1);

	if (x1 > x2 || y1 > y2) return;

	/* propagate redraw request to the parent */
	if (_parent)
		_parent->redraw_area(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}


Element *Element::find(Point position)
{
	if (position.x() >= _position.x() && position.x() < _position.x() + (int)_size.w()
	 && position.y() >= _position.y() && position.y() < _position.y() + (int)_size.h()
	 && _flags.findable)
		return this;

	return 0;
}


Element *Element::find_by_y(int y)
{
	return (y >= _position.y() && y < _position.y() + (int)_size.h()) ? this : 0;
}


Point Element::abs_position() const
{
	return _position + (_parent ? _parent->abs_position() : Point(0, 0));
}


Element *Element::chapter()
{
	if (_flags.chapter) return this;
	return _parent ? _parent->chapter() : 0;
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


void Parent_element::remove(Element const *e)
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


void Parent_element::forget(Element const *e)
{
	if (e->has_parent(this))
		remove(e);

	if (_parent)
		_parent->forget(e);
}


int Parent_element::_format_children(int x, int w)
{
	int y = 0;

	if (w <= 0) return 0;

	for (Element *e = _first; e; e = e->next) {
		e->format_fixed_width(w);
		e->geometry(Rect(Point(x, y), e->min_size()));
		y += e->min_size().h();
	}

	return y;
}


void Parent_element::draw(Canvas_base &canvas, Point abs_position)
{
	for (Element *e = _first; e; e = e->next)
		e->try_draw(canvas, abs_position + _position);
}


Element *Parent_element::find(Point position)
{
	/* check if position is outside the parent element */
	if (position.x() < _position.x() || position.x() >= _position.x() + (int)_size.w()
	 || position.y() < _position.y() || position.y() >= _position.y() + (int)_size.h())
		return 0;

	position = position - _position;

	/* check children */
	Element *ret = this;
	for (Element *e = _first; e; e = e->next) {
		Element *res  = e->find(position);
		if (res) ret = res;
	}

	return ret;
}


Element *Parent_element::find_by_y(int y)
{
	/* check if position is outside the parent element */
	if (y < _position.y() || y >= _position.y() + (int)_size.h())
		return 0;

	y -= _position.y();

	/* check children */
	for (Element *e = _first; e; e = e->next) {
		Element *res  = e->find_by_y(y);
		if (res) return res;
	}

	return this;
}


void Parent_element::geometry(Rect rect)
{
	::Element::geometry(rect);

	if (!_last || !_last->bottom()) return;

	_last->geometry(Rect(Point(_last->position().x(),
	                           rect.h() - _last->size().h()), _last->size()));
}


void Parent_element::fill_cache(Canvas_base &canvas)
{
	for (Element *e = _first; e; e = e->next) e->fill_cache(canvas);
}


void Parent_element::flush_cache(Canvas_base &canvas)
{
	for (Element *e = _first; e; e = e->next) e->flush_cache(canvas);
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
	_min_size = Area(_style->font->str_w(str, len) + _style->font->str_w(" ", 1),
	                 _style->font->str_h(str, len));
}


void Token::draw(Canvas_base &canvas, Point abs_position)
{
	if (!_style) return;
	if (_style->attr & Style::ATTR_BOLD)
		_outline = Color(_col.r, _col.g, _col.b, 32);

	abs_position = abs_position + Point(1, 1);

	if (_outline.a)
		for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++)
			canvas.draw_string(_position.x() + abs_position.x() + i,
			                   _position.y() + abs_position.y() + j,
			                   _style->font, _outline, _str, _len);

	canvas.draw_string(_position.x() + abs_position.x(),
	                   _position.y() + abs_position.y(),
	                   _style->font, _col, _str, _len);
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
	unsigned line_max_h = 0;
	unsigned max_w = 0;

	for (Element *e = _first; e; e = e->next) {

		/* wrap at the end of the line */
		if (x + (int)e->min_size().w() >= w) {
			x  = _second_indent;
			y += line_max_h;
			line_max_h = 0;
		}

		/* position element */
		if (max_w < x + e->min_size().w())
			max_w = x + e->min_size().w();

		e->geometry(Rect(Point(x, y), e->min_size()));

		/* determine token with the biggest height of the line */
		if (line_max_h < e->min_size().h())
			line_max_h = e->min_size().h();

		x += e->min_size().w();
	}

	/*
	 * Now, the text is left-aligned.
	 * Let's apply another alignment if specified.
	 */

	if (_align != LEFT) {
		for (Element *line = _first; line; ) {

			Element *e;
			int      cy = line->position().y();   /* y position of current line */
			int      max_x;                       /* rightmost position         */

			/* determine free space at the end of the line */
			for (max_x = 0, e = line; e && (e->position().y() == cy); e = e->next)
				max_x = max(max_x, e->position().x() + (int)e->size().w() - 1);

			/* indent elements of the line according to the alignment */
			int dx = 0;
			if (_align == CENTER) dx = max(0UL, (max_w - max_x)/2);
			if (_align == RIGHT)  dx = max(0UL, max_w - max_x);
			for (e = line; e && (e->position().y() == cy); e = e->next)
				e->geometry(Rect(Point(e->position().x() + dx, e->position().y()),
				                 e->size()));

			/* find first element of next line */
			for (; line && (line->position().y() == cy); line = line->next);
		}
	}

	/* line break at the end of the last line */
	if (line_max_h) y += line_max_h;

	_min_size = Area(max_w, y + 5);
}


/************
 ** Center **
 ************/

void Center::format_fixed_width(int w)
{
	_min_size = Area(_min_size.w(), _format_children(0, w));

	/* determine highest min with of children */
	unsigned highest_min_w = 0;
	for (Element *e = _first; e; e = e->next)
		if (highest_min_w < e->min_size().w())
			highest_min_w = e->min_size().w();

	unsigned dx = (w - highest_min_w)>>1;

	_min_size = Area(max((unsigned)w, highest_min_w), _min_size.h());

	/* move children to center */
	for (Element *e = _first; e; e = e->next)
		e->geometry(Rect(Point(dx, e->position().y()), e->size()));
}


/**************
 ** Verbatim **
 **************/

void Verbatim::draw(Canvas_base &canvas, Point abs_position)
{
	static const int pad = 5;

	canvas.draw_box(_position.x() + abs_position.x() + pad,
	                _position.y() + abs_position.x() + pad,
	                _size.w() - 2*pad, _size.h() - 2*pad, bgcol);

	int cx1 = canvas.clip().x1(), cy1 = canvas.clip().y1();
	int cx2 = canvas.clip().x2(), cy2 = canvas.clip().y2();

	canvas.clip(Rect(Point(_position.x() + abs_position.x() + pad,
	                       _position.y() + abs_position.y() + pad),
	                 Area(_size.w() - 2*pad, _size.h() - 2*pad)));
	Parent_element::draw(canvas, abs_position);

	canvas.clip(Rect(Point(cx1, cy1), Area(cx2 - cx1 + 1, cy2 - cy1 + 1)));
}


void Verbatim::format_fixed_width(int w)
{
	int y = 10;

	for (Element *e = _first; e; e = e->next) {

		/* position element */
		e->geometry(Rect(Point(10, y), e->min_size()));

		y += e->min_size().h();
	}

	_min_size = Area(w, y + 10);
}


/****************
 ** Link_token **
 ****************/

void Link_token::handle_event(Event const &e)
{
	if (e.type != Event::PRESS) return;

	/* lookup browser, in which the link token resides */
	Browser *b = 0;
	for (Element *e = this; e && !b; e = e->parent())
		b = dynamic_cast<Browser *>(e);

	/* let browser follow link */
	if (b && _dst) b->go_to(_dst);
}


/*************************
 ** Launcher_link_token **
 *************************/

void Launcher_link_token::handle_event(Event const &e)
{
	if (e.type != Event::PRESS) return;

	step(8);
	curr(255);

	/* launch executable */
	Launcher *l = (Launcher *)_dst;
	if (l) l->launch();
}
