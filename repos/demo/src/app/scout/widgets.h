/*
 * \brief   Basic user interface elements
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WIDGETS_H_
#define _WIDGETS_H_

#include "elements.h"

namespace Scout {
	class Docview;
	template <typename, int> class Horizontal_shadow;
	class Generic_icon;
	template <typename, int, int> class Icon;
}


class Scout::Docview : public Parent_element
{
	private:

		/*
		 * Noncopyable
		 */
		Docview(Docview const &);
		Docview &operator = (Docview const &);

		Element *_bg;
		Element *_cont;
		int      _voffset;
		int      _right_pad;
		int      _padx;

	public:

		/**
		 * Constructor
		 */
		explicit Docview(int padx = 7) :
			_bg(0), _cont(0), _voffset(0), _right_pad(0), _padx(padx) { }

		/**
		 * Accessor functions
		 */
		Element *content() { return _cont; }

		/**
		 * Define content to be presented in the Docview
		 */
		inline void content(Element *cont)
		{
			_cont = cont;
			_last = _first = 0;
			append(cont);
		}

		inline void voffset(int voffset) { _voffset = voffset; }

		/**
		 * Define background texture
		 */
		inline void texture(Element *bg) { _bg = bg; }

		/**
		 * Define right padding
		 */
		inline void right_pad(int pad) { _right_pad = pad; }

		/**
		 * Element interface
		 */
		void     format_fixed_width(int)    override;
		void     draw(Canvas_base &, Point) override;
		Element *find(Point)                override;
		void     geometry(Rect)             override;
};


template <typename PT, int INTENSITY>
struct Scout::Horizontal_shadow : Element
{
	explicit Horizontal_shadow(int height = 8)
	{
		_min_size = Area(0, height);
	}

	/**
	 * Element interface
	 */
	void draw(Canvas_base &, Point) override;
	Element *find(Point) override { return 0; }
	void format_fixed_width(int w) override { _min_size = Area(w, _min_size.h()); }
};


class Scout::Generic_icon : public Element
{
	public:

		/**
		 * Request current alpha value
		 */
		virtual int alpha() = 0;

		/**
		 * Define alpha value of the icon
		 */
		virtual void alpha(int alpha) = 0;
};


template <typename PT, int W, int H>
class Scout::Icon : public Generic_icon
{
	private:

		PT            _pixel  [H][W];     /* icon pixels in PT pixel format */
		unsigned char _alpha  [H][W];     /* alpha channel of icon pixels   */
		unsigned char _shadow [H][W];     /* shadow calculation buffer      */
		int           _icon_alpha = 255;  /* alpha value of whole icon      */

	public:

		/**
		 * Constructor
		 */
		Icon();

		/**
		 * Define new icon pixels from rgba buffer
		 *
		 * \param vshift  vertical shift of pixels
		 * \param shadow  shadow divisor, low value -> dark shadow
		 *                special case zero -> no shadow
		 *
		 * The buffer must contains W*H pixels. Each pixels consists
		 * of four bytes, red, green, blue, and alpha.
		 */
		void rgba(unsigned char *src, int vshift = 0, int shadow = 4);

		/**
		 * Define icon to be a glow of an rgba image
		 *
		 * \param src  source rgba image to extract the glow's shape from
		 * \param c    glow color
		 */
		void glow(unsigned char *src, Color c);

		/**
		 * Generic_icon interface
		 */
		int alpha() override { return _icon_alpha; }

		void alpha(int alpha) override
		{
			_icon_alpha = alpha;
			refresh();
		}

		/**
		 * Element interface
		 */
		void draw(Canvas_base &, Point) override;
		Element *find(Point) override;
};


#endif /* _WIDGETS_H_ */
