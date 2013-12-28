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
#include <nitpicker_gfx/canvas.h>
#include <os/session_policy.h>

/* local includes */
#include "color.h"

class Texture;
class View;
class Session;

namespace Input { class Event; }

typedef Genode::List<Session> Session_list;

class Session : public Session_list::Element
{
	private:

		Genode::Session_label const  _label;
		Genode::Color                _color;
		Texture               const *_texture;
		View                        *_background = 0;
		int                          _v_offset;
		unsigned char         const *_input_mask;
		bool                  const  _stay_top;

	public:

		/**
		 * Constructor
		 *
		 * \param label     session label
		 * \param v_offset  vertical screen offset of session
		 * \param stay_top  true for views that stay always in front
		 */
		Session(Genode::Session_label const &label, int v_offset, bool stay_top)
		:
			_label(label), _texture(0), _v_offset(v_offset),
			_input_mask(0), _stay_top(stay_top)
		{ }

		virtual ~Session() { }

		virtual void submit_input_event(Input::Event ev) = 0;

		Genode::Session_label const &label() const { return _label; }

		Texture const *texture() const { return _texture; }

		void texture(Texture const *texture) { _texture = texture; }

		/**
		 * Set input mask buffer
		 *
		 * \param mask  input mask buffer containing a byte value per texture
		 *              pixel, which describes the policy of handling user
		 *              input referring to the pixel. If set to zero, the input
		 *              is passed through the view such that it can be handled
		 *              by one of the subsequent views in the view stack. If
		 *              set to one, the input is consumed by the view. If
		 *              'input_mask' is a null pointer, user input is
		 *              unconditionally consumed by the view.
		 */
		void input_mask(unsigned char const *mask) { _input_mask = mask; }

		Genode::Color color() const { return _color; }

		View *background() const { return _background; }

		void background(View *background) { _background = background; }

		bool stay_top() const { return _stay_top; }

		/**
		 * Return true if session uses an alpha channel
		 */
		bool uses_alpha() const { return _texture ? _texture->alpha() : 0; }

		/**
		 * Return vertical offset of session
		 */
		int v_offset() const { return _v_offset; }

		/**
		 * Return input mask value at specified buffer position
		 */
		unsigned char input_mask_at(Canvas::Point p) const
		{
			if (!_input_mask || !_texture) return 0;

			/* check boundaries */
			if ((unsigned)p.x() >= _texture->w()
			 || (unsigned)p.y() >= _texture->h())
				return 0;

			return _input_mask[p.y()*_texture->w() + p.x()];
		}

		/**
		 * Set session color according to the list of configured policies
		 *
		 * Select the policy that matches the label. If multiple policies
		 * match, select the one with the largest number of characters.
		 */
		void apply_session_color()
		{
			/* use white by default */
			_color = WHITE;

			try {
				Genode::Session_policy policy(_label);

				/* read color attribute */
				policy.attribute("color").value(&_color);
			} catch (...) { }
		}
};

#endif
