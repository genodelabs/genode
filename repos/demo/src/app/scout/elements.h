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

#ifndef _ELEMENTS_H_
#define _ELEMENTS_H_

#include <scout/parent_element.h>
#include <scout/printf.h>
#include <scout/string.h>
#include <scout/fader.h>
#include <scout/platform.h>

namespace Scout {
	class Style;
	class Browser;
	class Token;
	class Link;
	class Link_token;
	class Launcher_config;
	class Launcher;
	class Launcher_link_token;
	class Block;
	class Center;
	class Png_image;
	class Chapter;
	class Document;
	class Spacer;
	class Verbatim;
	class Item;
	class Generic_icon;
	class Navbar;

	/**
	 * Link anchor
	 *
	 * An anchor marks a location within a document that can be addressed by a
	 * link.
	 */
	typedef Element Anchor;
}


/**
 * Textual style
 *
 * A style describes the font, color and accentuations of tokens.
 */
class Scout::Style
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


/**
 * String token
 *
 * A Token is a group of characters that are handled as an atomic text unit.
 * Line wrapping is performed at the granularity of tokens.
 */
class Scout::Token : public Element
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
		void draw(Canvas_base &, Point);
		void refresh() { redraw_area(-1, 0, _size.w() + 1, _size.h()); }
};


/**
 * Link that references an anchor within the document
 */
class Scout::Link
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
class Scout::Link_token : public Token, public Link, public Event_handler,
                          public Fader
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
			_curr_value        = 0;
			event_handler(this);
		}
		
		/**
		 * Element interface
		 */
		void draw(Canvas_base &canvas, Point abs_position)
		{
			_outline = Color(_style->color.r,
			                 _style->color.g,
			                 _style->color.b, _curr_value);

			Token::draw(canvas, abs_position);

			canvas.draw_box(_position.x() + abs_position.x(),
			                _position.y() + abs_position.y() + _size.h() - 1,
			                _size.w(), 1, Color(0,0,255));
		}

		void mfocus(int flag)
		{
			/*
			 * highlight link of all siblings that point to the same link.
			 */
			if (_dst) {
				Element const *dst = _dst;
				for_each_sibling([dst, flag] (Element &e) {
					Link_token *l = dynamic_cast<Link_token *>(&e);
					if (l && l->has_destination(dst))
						l->highlight_link(flag);
				});
			}

			Token::mfocus(flag);
		}

		void highlight_link(bool flag)
		{
			if (flag && _curr_value != _MAX_ALPHA)
				fade_to(_MAX_ALPHA, 50);

			if (!flag && _curr_value != 0)
				fade_to(0, 2);
		}

		bool has_destination(Element const *e) { return e == _dst; }

		/**
		 * Event handler interface
		 */
		void handle_event(Event const &) override;

		/**
		 * Tick interface
		 */
		int on_tick()
		{
			/* call on_tick function of the fader */
			if (Fader::on_tick() == 0) return 0;

			refresh();
			return 1;
		}
};


class Launchpad;


class Scout::Launcher : public Anchor
{
	public:

		typedef Genode::String<64> Name;

	private:

		Name                _prg_name;
		int                 _active;
		int                 _exec_once;
		Launchpad          *_launchpad;
		unsigned long const _caps;
		unsigned long       _quota;
		Launcher_config    *_config;

	public:

		static void init(Genode::Env &, Genode::Allocator &);

		/**
		 * Constructors
		 */
		Launcher(Name const &prg_name, int exec_once = 0,
		         unsigned long caps = 0, unsigned long quota = 0,
		         Launcher_config *config = 0)
		:
			_prg_name(prg_name), _active(1),
			_exec_once(exec_once), _caps(caps), _quota(quota), _config(config)
		{ }

		Launcher(Name const &prg_name, Launchpad *launchpad,
		         unsigned long caps, unsigned long quota,
		         Launcher_config *config = 0)
		:
			_prg_name(prg_name), _launchpad(launchpad),
			_caps(caps), _quota(quota), _config(config)
		{ }

		int active() const { return _active; }

		Name prg_name() const { return _prg_name; }

		void quota(unsigned long quota) { _quota = quota; }

		unsigned long quota() const { return _quota; }

		unsigned long caps() const { return _caps; }

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
class Scout::Launcher_link_token : public Link_token
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
		void handle_event(Event const &) override;
};


/**
 * Text block
 *
 * A block is a group of tokens that form a paragraph. A block layouts its
 * tokens while using line wrapping.
 */
class Scout::Block : public Parent_element
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
class Scout::Center : public Parent_element
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
class Scout::Png_image : public Element
{
	private:

		void         *_png_data;
		Texture_base *_texture;

	public:

		static void init(Genode::Allocator &);

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
		void fill_cache(Canvas_base &);
		void flush_cache(Canvas_base &);
		void draw(Canvas_base &, Point);
};


/**
 * Document
 */
class Scout::Document : public Parent_element
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
			_min_size = Area(w, _format_children(0, w));
		}
};


/**
 * Chapter
 */
class Scout::Chapter : public Document { };


/**
 * Spacer
 *
 * A spacer is a place holder that consumes some screen space. It is used for
 * tweaking the layout of the document.
 */
struct Scout::Spacer : Element
{
	Spacer(int w, int h) { _min_size = _size = Area(w, h); }
};


/**
 * Verbatim text block
 *
 * A verbatim text block consists of a number of preformatted text lines.
 * The text is printed in a monospaced font and the whole verbatim area
 * has a shaded background.
 */
class Scout::Verbatim : public Parent_element
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
		void draw(Canvas_base &, Point);
		void format_fixed_width(int);
};


/**
 * An iten consists of a item tag and a list of blocks
 */
class Scout::Item : public Parent_element
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
			_min_size = Area(w, _format_children(_tag_ident, w - _tag_ident));
		}

		void draw(Canvas_base &canvas, Point abs_position)
		{
			canvas.draw_string(_position.x() + abs_position.x(),
			                   _position.y() + abs_position.y(),
			                   _style->font, _style->color, _tag, 255);
			Parent_element::draw(canvas, abs_position);
		}
};


/**
 * Document navigation bar
 */
class Scout::Navbar : public Parent_element, public Fader
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
		void format_fixed_width(int);
		void draw(Canvas_base &, Point);
		Element *find(Point);

		/**
		 * Tick interface
		 */
		int on_tick();
};

#endif /* _ELEMENTS_H_ */
