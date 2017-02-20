/*
 * \brief   Implementation of fading icon
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * Fading icons are presented at a alpha value of 50 percent.
 * When getting the mouse focus, we smoothly increase the
 * alpha value to 100 percent.
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FADE_ICON_H_
#define _FADE_ICON_H_

#include <scout/misc_math.h>
#include <scout/fader.h>
#include "widgets.h"

namespace Scout { template <typename PT, int W, int H> class Fade_icon; }


template <typename PT, int W, int H>
class Scout::Fade_icon : public Fader, public Icon<PT, W, H>
{
	private:

		int _default_alpha;
		int _focus_alpha;

	public:

		/**
		 * Constructor
		 */
		Fade_icon()
		{
			_curr_value = _dst_value = _default_alpha = 100;
			_focus_alpha = 255;
			step(12);
		}

		/**
		 * Accessor functions
		 */
		int default_alpha() { return _default_alpha; }

		/**
		 * Define alpha value for unfocused icon
		 */
		void default_alpha(int alpha ) { _default_alpha = alpha; }

		/**
		 * Define alpha value when having the mouse focus
		 */
		void focus_alpha(int alpha) { _focus_alpha = alpha; }

		/**
		 * Tick interface
		 */
		int on_tick()
		{
			/* call on_tick function of the fader */
			if (Fader::on_tick() == 0) return 0;

			Icon<PT, W,H>::alpha(_curr_value);
			return 1;
		}

		/**
		 * Icon interface
		 */
		void alpha(int alpha)
		{
			_curr_value = alpha;
			Icon<PT, W, H>::alpha(alpha);
		}

		/**
		 * Element interface
		 */
		void mfocus(int flag)
		{
			Icon<PT, W, H>::mfocus(flag);
			int step = _focus_alpha - _default_alpha;
			step *= flag ? 26 : 19;
			fade_to(flag ? _focus_alpha : _default_alpha, step >> 8);
		}
};


#endif /* _FADE_ICON_H_ */
