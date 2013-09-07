/*
 * \brief  Nitpicker session interface
 * \author Norman Feske
 * \date   2006-08-09
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SESSION_H_
#define _SESSION_H_

/* Genode includes */
#include <util/list.h>
#include <util/string.h>

class Texture;
class View;
class Session;

namespace Input { class Event; }

typedef Genode::List<Session> Session_list;

class Session : public Session_list::Element
{
	public:

		enum { LABEL_LEN = 64 };  /* max. length of session label */

	private:

		char                 _label[LABEL_LEN];
		Color                _color;
		Texture       const &_texture;
		View                *_background;
		int                  _v_offset;
		unsigned char const *_input_mask;
		bool          const  _stay_top;

	public:

		/**
		 * Constructor
		 *
		 * \param label       textual session label as null-terminated
		 *                    ASCII string
		 * \param texture     texture containing the session's pixel
		 *                    representation
		 * \param v_offset    vertical screen offset of session
		 * \param input_mask  input mask buffer containing a byte value per
		 *                    texture pixel, which describes the policy of
		 *                    handling user input referring to the pixel.
		 *                    If set to zero, the input is passed through
		 *                    the view such that it can be handled by one of
		 *                    the subsequent views in the view stack. If set
		 *                    to one, the input is consumed by the view. If
		 *                    'input_mask' is a null pointer, user input is
		 *                    unconditionally consumed by the view.
		 */
		Session(char const *label, Texture const &texture, int v_offset,
		        Color color, unsigned char const *input_mask = 0,
		        bool stay_top = false)
		:
			_color(color), _texture(texture), _background(0),
			_v_offset(v_offset), _input_mask(input_mask), _stay_top(stay_top) {
			Genode::strncpy(_label, label, sizeof(_label)); }

		virtual ~Session() { }

		virtual void submit_input_event(Input::Event ev) = 0;

		char const *label() const { return _label; }

		Texture const &texture() const { return _texture; }

		Color color() const { return _color; }

		View *background() const { return _background; }

		void background(View *background) { _background = background; }

		bool stay_top() const { return _stay_top; }

		/**
		 * Return true if session uses an alpha channel
		 */
		bool uses_alpha() const { return _texture.alpha(); }

		/**
		 * Return vertical offset of session
		 */
		int v_offset() const { return _v_offset; }

		/**
		 * Return input mask value at specified buffer position
		 */
		unsigned char input_mask_at(Point p) const
		{
			if (!_input_mask) return 0;

			/* check boundaries */
			if (p.x() < 0 || p.x() >= _texture.w()
			 || p.y() < 0 || p.y() >= _texture.h())
				return 0;

			return _input_mask[p.y()*_texture.w() + p.x()];
		}
};

#endif
