/*
 * \brief  Oklx library specific screen data
 * \author Stefan Kalkowski
 * \date   2010-04-22
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIB__OKLX__INCLUDE__OKLX_SCREENS_H_
#define _LIB__OKLX__INCLUDE__OKLX_SCREENS_H_

#include <util/list.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <input/event.h>
#include <nitpicker_session/connection.h>


class Screen : public Genode::List<Screen>::Element
{
	private:

		void         *_mouse_dev;
		void         *_keyb_dev;

	protected:

		Input::Event *_ev_buf;

	public:

		Screen() : _mouse_dev(0), _keyb_dev(0), _ev_buf(0) {}

		virtual Framebuffer::Session *framebuffer() = 0;
		virtual Input::Session *input() = 0;

		void *keyb_device (void)      { return _keyb_dev;  }
		void *mouse_device (void)     { return _mouse_dev; }
		void keyb_device (void *dev)  { _keyb_dev  = dev;  }
		void mouse_device (void *dev) { _mouse_dev = dev;  }
		Input::Event *buffer (void)   { return _ev_buf;    }
};


class Simple_screen : public Screen
{
	private:

		Framebuffer::Connection _fb_con;
		Input::Connection       _input_con;

	public:

		Simple_screen()
		{
			_ev_buf = (Input::Event*)
				Genode::env()->rm_session()->attach(_input_con.dataspace());
		}

		Framebuffer::Session *framebuffer() { return &_fb_con; }
		Input::Session *input() { return &_input_con; }
};

class Nitpicker_screen : public Screen
{
	public:

		enum { VIEW_CNT=256 };

	private:

		Nitpicker::Connection      _nit_con;
		Nitpicker::View_capability _views[VIEW_CNT];

	public:

		Nitpicker_screen()
		{
			_ev_buf = (Input::Event*)
				Genode::env()->rm_session()->attach(_nit_con.input()->dataspace());
			for (unsigned i = 0; i < VIEW_CNT; i++)
				_views[i] = Nitpicker::View_capability();
		}

		Framebuffer::Session *framebuffer() { return _nit_con.framebuffer(); }

		Input::Session *input() { return _nit_con.input(); }

		Nitpicker::Connection *nitpicker() { return &_nit_con;}

		Nitpicker::View_capability get_view (unsigned idx)
		{
			if (idx >= VIEW_CNT)
				return Nitpicker::View_capability();
			return _views[idx];
		}

		void put_view (unsigned idx, Nitpicker::View_capability view)
		{
			if (idx < VIEW_CNT)
				_views[idx] = view;
		}
};


class Screen_array
{
	public:

		enum { SIZE=10 };

	private:

		Screen *_screens[SIZE];

	public:

		Screen_array();

		Screen *get (unsigned int idx)
		{
			if (idx >= SIZE) {
				PWRN("Invalid index %d", idx);
				return (Screen*) 0;
			}
			return _screens[idx];
		}

		unsigned count ()
		{
			for (unsigned i = 0; i < SIZE; i++) {
				if (!_screens[i])
					return i;
			}
			return SIZE;
		}

		static Screen_array *screens(void);
};

#endif //_LIB__OKLX__INCLUDE__OKLX_SCREENS_H_
