/*
 * \brief  Nitpicker menubar interface
 * \author Norman Feske
 * \date   2006-08-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MENUBAR_H_
#define _MENUBAR_H_

#include "view.h"
#include "draw_label.h"
#include "mode.h"

struct Menubar_state
{
	Genode::String<128> session_label;
	Mode                mode;
	Color               session_color;

	Menubar_state(Mode mode, char const *session_label, Color session_color)
	:
		session_label(session_label), mode(mode), session_color(session_color)
	{ }

	Menubar_state() : session_color(BLACK) { }
};


struct Menubar : Menubar_state
{
	virtual ~Menubar() { }

	/**
	 * Set state that is displayed in the trusted menubar
	 */
	virtual void state(Menubar_state) = 0;

	Menubar_state state() const { return *this; }
};

#endif
