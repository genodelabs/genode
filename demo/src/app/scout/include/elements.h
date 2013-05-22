/*
 * \brief   Document structure elements
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ELEMENTS_H_
#define _ELEMENTS_H_

#include "printf.h"
#include "string.h"
#include "canvas.h"
#include "event.h"
#include "color.h"
#include "fader.h"
#include "font.h"


/**
 * Textual style
 *
 * A style describes the font, color and accentuations of tokens.
 */
class Style
{
	public:

		enum {
			ATTR_BOLD = 0x1,
		};

		Font  *font;
		Color  color;
		int    attr;

		Style(Font *f, Color c, int a)
		{
			font  = f;
			color = c;
			attr  = a;
		}
};


class Parent_element;
class Browser;

class Element
{
	protected:

		int _x, _y;                   /* relative position managed by parent */
		int _w, _h;                   /* size managed by parent              */
		int _min_w, _min_h;           /* min size managed by element         */
		Parent_element *_parent;      /* parent in element hierarchy         */
		Event_handler  *_evh;         /* event handler object                */
		struct {
			int mfocus      : 1;      /* element has mouse focus             */
			int selected    : 1;      /* element has selected state          */
			int takes_focus : 1;      /* element highlights mouse focus      */
			int link        : 1;      /* element is a link                   */
			int chapter     : 1;      /* display element as single page      */
			int findable    : 1;      /* regard element in find function     */
			int bottom      : 1;      /* place element to the bottom         */
		} _flags;

	public:

		Element *next;              /* managed by parent */

		/**
		 * Constructor
		 */
		Element()
		{
			next    = 0;
			_parent = 0;
			_min_w  = 0;
			_min_h  = 0;
			_x = _y = 0;
			_w = _h = 0;
			_evh    = 0;
			_flags.mfocus      = _flags.selected = 0;
			_flags.takes_focus = _flags.link     = 0;
			_flags.chapter     = 0;
			_flags.findable    = 1;
			_flags.bottom      = 0;
		}

		/**
		 * Destructor
		 */
		virtual ~Element();

		/**
		 * Accessor functionse
		 */
		inline int min_w()     { return _min_w; }
		inline int min_h()     { return _min_h; }
		inline int x()         { return _x; }
		inline int y()         { return _y; }
		inline int w()         { return _w; }
		inline int h()         { return _h; }
		inline int is_link()   { return _flags.link; }
		inline int is_bottom() { return _flags.bottom; }

		inline void findable(int flag) { _flags.findable = flag; }

		/**
		 * Set geometry of the element
		 *
		 * This function should only be called by the immediate parent
		 * element.
		 */
		virtual void geometry(int x, int y, int w, int h)
		{
			_x = x; _y = y; _w = w; _h = h;
		}

		/**
		 * Set/reset the mouse focus
		 */
		virtual void mfocus(int flag)
		{
			if ((_flags.mfocus == flag) || !_flags.takes_focus) return;
			_flags.mfocus = flag;
			refresh();
		}

		/**
		 * Define/request parent of an element
		 */
		inline void parent(Parent_element *parent) { _parent = parent; }
		inline Parent_element *parent() { return _parent; }

		/**
		 * Define event handler object
		 */
		inline void event_handler(Event_handler *evh) { _evh = evh; }

		/**
		 * Check if element is completely clipped and draw it otherwise
		 */
		inline void try_draw(Canvas *c, int x, int y)
		{
			/* check if element is completely outside the clipping area */
			if ((_x + x > c->clip_x2()) || (_x + x + _w - 1 < c->clip_x1())
			 || (_y + y > c->clip_y2()) || (_y + y + _h - 1 < c->clip_y1()))
				return;

			/* call actual drawing function */
			draw(c, x, y);
		}

		/**
		 * Format element and all child elements to specified width
		 */
		virtual void format_fixed_width(int w) { }

		/**
		 * Format element and all child elements to specified width and height
		 */
		virtual void format_fixed_size(int w, int h) { }

		/**
		 * Draw function
		 *
		 * This function must not be called directly.
		 * Instead, the function try_draw should be called.
		 */
		virtual void draw(Canvas *c, int x, int y) { }

		/**
		 * Find top-most element at specified position
		 *
		 * The default implementation can be used for elements without
		 * children. It just the element position and size against the
		 * specified position.
		 */
		virtual Element *find(int x, int y);

		/**
		 * Find the back-most element at specified y position
		 *
		 * This function is used to query a document element at
		 * the current scroll position of the window. This way,
		 * we can adjust the y position to the right value
		 * when we browse the history.
		 */
		virtual Element *find_by_y(int y);

		/**
		 * Request absolute position of an element
		 */
		int abs_x();
		int abs_y();

		/**
		 * Update area of an element on screen
		 *
		 * We propagate the redraw request through the element hierarchy to
		 * the parent. The root parent should overwrite this function with
		 * a function that performs the actual redraw.
		 */
		virtual void redraw_area(int x, int y, int w, int h);

		/**
		 * Trigger the refresh of an element on screen
		 */
		inline void refresh() { redraw_area(0, 0, _w, _h); }

		/**
		 * Handle user input or timer event
		 */
		inline void handle_event(Event &ev) { if (_evh) _evh->handle(ev); }

		/**
		 * Request the chapter in which the element lives
		 */
		Element *chapter();

		/**
		 * Request the browser in which the element lives
		 */
		virtual Browser *browser();

		/**
		 * Fill image cache for element
		 */
		virtual void fill_cache(Canvas *c) { }

		/**
		 * Flush image cache for element
		 */
		virtual void flush_cache(Canvas *c) { }

		/**
		 * Propagate current link destination
		 *
		 * Elements that reference the specified link destination
		 * should give feedback. Other elements should ignore this
		 * function.
		 */
		virtual void curr_link_destination(Element *e) { }
};


class Parent_element : public Element
{
	protected:

		Element *_first;
		Element *_last;

		/**
		 * Format child element by a given width an horizontal offset
		 */
		int _format_children(int x, int w);

	public:

		/**
		 * Constructor
		 */
		Parent_element() { _first = _last = 0; }

		/**
		 * Adopt a child element
		 */
		void append(Element *e);

		/**
		 * Release child element from parent element
		 */
		void remove(Element *e);

		/**
		 * Dispose references to the specified element
		 *
		 * The element is not necessarily an immediate child but some element
		 * of the element-subtree.  This function gets propagated to the root
		 * parent (e.g., user state manager), which can reset the mouse focus
		 * of the focused element vanishes.
		 */
		virtual void forget(Element *e);

		/**
		 * Element interface
		 */
		void     draw(Canvas *c, int x, int y);
		Element *find(int x, int y);
		Element *find_by_y(int y);
		void     fill_cache(Canvas *c);
		void     flush_cache(Canvas *c);
		void     curr_link_destination(Element *e);
		void     geometry(int x, int y, int w, int h);
};


/**
 * String token
 *
 * A Token is a group of characters that are handled as an atomic text unit.
 * Line wrapping is performed at the granularity of tokens.
 */
class Token : public Element
{
	protected:

		const char  *_str;       /* start  of string   */
		int          _len;       /* length of string   */
		Style       *_style;     /* textual style      */
		Color        _col;       /* current text color */
		Color        _outline;   /* outline color      */

	public:

		/**
		 * Constructor
		 */
		Token(Style *style, const char *str, int len);

		/**
		 * Element interface
		 */
		void draw(Canvas *c, int x, int y);
		inline void refresh() { redraw_area(-1, 0, _w + 1, _h); }
};


/**
 * Link anchor
 *
 * An anchor marks a location within a document that can be addressed by a
 * link.
 */
typedef Element Anchor;


/**
 * Link that references an anchor within the document
 */
class Link
{
	protected:

		Anchor *_dst;  /* link destination */

	public:

		/**
		 * Constructor
		 */
		explicit Link(Anchor *dst) { _dst = dst; }

		/**
		 * Accessor function
		 */
		Anchor *dst() { return _dst; }
};


/**
 * Textual link
 */
class Link_token : public Token, public Link, public Event_handler, public Fader
{
	private:

		enum { _MAX_ALPHA = 50 };

	public:

		/**
		 * Constructor
		 */
		Link_token(Style *style, const char *str, int len, Anchor *dst)
		: Token(style, str, len), Link(dst)
		{
			_flags.takes_focus = 1;
			_flags.link        = 1;
			_curr_value        = 0;
			event_handler(this);
		}
		
		/**
		 * Element interface
		 */
		void draw(Canvas *c, int x, int y)
		{
			_outline.rgba(_style->color.r,
			              _style->color.g,
			              _style->color.b, _curr_value);

			::Token::draw(c, x, y);
		}

		void curr_link_destination(Element *dst)
		{
			if (dst == _dst && _curr_value != _MAX_ALPHA)
				fade_to(_MAX_ALPHA, 50);

			if (dst != _dst && _curr_value != 0)
				fade_to(0, 2);
		}

		/**
		 * Event handler interface
		 */
		void handle(Event &e);

		/**
		 * Tick interface
		 */
		int on_tick()
		{
			/* call on_tick function of the fader */
			if (::Fader::on_tick() == 0) return 0;

			refresh();
			return 1;
		}
};


class Launchpad;
class Launcher_config;
class Launcher : public Anchor
{
	private:

		const char      *_prg_name;  /* null-terminated name of the program */
		int              _active;
		int              _exec_once;
		Launchpad       *_launchpad;
		unsigned long    _quota;
		Launcher_config *_config;

	public:

		/**
		 * Constructors
		 */
		Launcher(const char *prg_name, int exec_once = 0,
		         unsigned long quota = 0, Launcher_config *config = 0) :
			_prg_name(prg_name), _active(1),
			_exec_once(exec_once), _quota(quota), _config(config) { }

		Launcher(const char *prg_name, Launchpad *launchpad,
		         unsigned long quota, Launcher_config *config = 0) :
			_prg_name(prg_name), _launchpad(launchpad), _quota(quota),
			_config(config) { }

		int active() { return _active; }

		const char *prg_name() { return _prg_name; }

		void quota(unsigned long quota) { _quota = quota; }

		unsigned long quota() { return _quota; }

		Launcher_config *config() { return _config; }

		/**
		 * Launch program
		 */
		void launch();
};


/**
 * Executable launcher link
 *
 * This is a special link that enables us to start external applications.
 */
class Launcher_link_token : public Link_token
{
	public:

		/**
		 * Constructor
		 */
		Launcher_link_token(Style *style, const char *str, int len, Launcher *l)
		: Link_token(style, str, len, l) { }

		/**
		 * Event handler interface
		 */
		void handle(Event &e);
};


/**
 * Text block
 *
 * A block is a group of tokens that form a paragraph. A block layouts its
 * tokens while using line wrapping.
 */
class Block : public Parent_element
{
	public:

		enum Alignment { LEFT, CENTER, RIGHT };
		enum Text_type { PLAIN, LINK, LAUNCHER };

	private:

		int       _second_indent;   /* indentation of second line */
		Alignment _align;           /* text alignment             */

		/**
		 * Append text to block
		 */
		void append_text(const char *str, Style *style, Text_type,
		                 Anchor *a, Launcher *l);

	public:

		/**
		 * Constructors
		 */
		explicit Block(int second_indent = 0)
		{
			_align         = LEFT;
			_second_indent = second_indent;
		}

		explicit Block(Alignment align)
		{
			_align         = align;
			_second_indent = 0;
		}

		/**
		 * Define alignment of text
		 */
		void align(Alignment);

		/**
		 * Append a string of space-separated words
		 */
		void append_plaintext(const char *str, Style *style)
		{
			append_text(str, style, PLAIN, 0, 0);
		}

		/**
		 * Append a string of space-separated words a link
		 *
		 * \param dst  anchor that defines the link destination
		 */
		void append_linktext(const char *str, Style *style, Anchor *a)
		{
			append_text(str, style, LINK, a, 0);
		}

		/**
		 * Append a string of space-separated words a launcher-link
		 */
		void append_launchertext(const char *str, Style *style, Launcher *l)
		{
			append_text(str, style, LAUNCHER, 0, l);
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w);
};


/**
 * Horizontally centered content
 */
class Center : public Parent_element
{
	public:

		/**
		 * Constructor
		 */
		explicit Center(Element *content = 0)
		{
			if (content) append(content);
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w);
};


/**
 * PNG Image
 */
class Png_image : public Element
{
	private:

		void            *_png_data;
		Canvas::Texture *_texture;

	public:

		/**
		 * Constructor
		 */
		explicit Png_image(void *png_data)
		{
			_png_data = png_data;
			_texture  = 0;
		}

		/**
		 * Accessor functions
		 */
		inline void *png_data() { return _png_data; }

		/**
		 * Element interface
		 */
		void fill_cache(Canvas *c);
		void flush_cache(Canvas *c);
		void draw(Canvas *c, int x, int y);
};


/**
 * Document
 */
class Chapter;
class Document : public Parent_element
{
	public:

		Chapter    *toc;     /* table of contents */
		const char *title;   /* document title    */

		/**
		 * Constructor
		 */
		Document()
		{
			toc = 0; title = ""; _flags.chapter = 1;
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w)
		{
			_min_h = _format_children(0, w);
			_min_w = w;
		}
};


/**
 * Chapter
 */
class Chapter : public Document { };


/**
 * Spacer
 *
 * A spacer is a place holder that consumes some screen space. It is used for
 * tweaking the layout of the document.
 */
class Spacer : public Element
{
	public:

		/**
		 * Constructor
		 */
		Spacer(int w, int h)
		{
			_min_w = _w = w;
			_min_h = _h = h;
		}
};


/**
 * Verbatim text block
 *
 * A verbatim text block consists of a number of preformatted text lines.
 * The text is printed in a monospaced font and the whole verbatim area
 * has a shaded background.
 */
class Verbatim : public Parent_element
{
	public:

		Color bgcol;

		/**
		 * Constructor
		 */
		explicit Verbatim(Color bg) { bgcol = bg; }

		/**
		 * Append verbatim text line
		 */
		void append_textline(const char *str, Style *style)
		{
			append(new Token(style, str, strlen(str)));
		}

		/**
		 * Element interface
		 */
		void draw(Canvas *c, int x, int y);
		void format_fixed_width(int w);
};


/**
 * An iten consists of a item tag and a list of blocks
 */
class Item : public Parent_element
{
	public:

		int         _tag_ident;
		const char *_tag;
		Style      *_style;

		/**
		 * Constructor
		 */
		Item(Style *style, const char *str, int ident)
		{
			_style     = style;
			_tag       = str;
			_tag_ident = ident;
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w)
		{
			_min_h = _format_children(_tag_ident, w - _tag_ident);
			_min_w = w;
		}

		void draw(Canvas *c, int x, int y)
		{
			c->draw_string(_x + x, _y + y, _style->font, _style->color, _tag, 255);
			Parent_element::draw(c, x, y);
		}
};


/**
 * Document navigation bar
 */
class Generic_icon;
class Navbar : public Parent_element, public Fader
{
	private:

		Block *_next_title;
		Block *_prev_title;

		Anchor *_next_anchor;
		Anchor *_prev_anchor;

	public:

		/**
		 * These pointers must be initialized such that they
		 * point to valid Icon widgets that are used as graphics
		 * for the navigation bar.
		 */
		static Generic_icon *next_icon;
		static Generic_icon *prev_icon;
		static Generic_icon *nbox_icon;
		static Generic_icon *pbox_icon;

		/**
		 * Constructor
		 */
		Navbar();

		/**
		 * Define link to next and previous chapter
		 */
		void next_link(const char *title, Anchor *dst);
		void prev_link(const char *title, Anchor *dst);

		/**
		 * Element interface
		 */
		void format_fixed_width(int w);
		void draw(Canvas *c, int x, int y);
		Element *find(int x, int y);

		/**
		 * Tick interface
		 */
		int on_tick();
};

#endif /* _ELEMENTS_H_ */
