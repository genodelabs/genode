/*
 * \brief  Nitpicker test program
 * \author Norman Feske
 * \date   2006-08-23
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <util/list.h>
#include <base/component.h>
#include <base/log.h>
#include <nitpicker_session/connection.h>
#include <timer_session/connection.h>
#include <input/event.h>

using namespace Genode;

class Test_view : public List<Test_view>::Element
{
	private:

		typedef Nitpicker::Session::View_handle View_handle;
		typedef Nitpicker::Session::Command     Command;

		Nitpicker::Session_client &_nitpicker;
		View_handle                _handle { };
		int                        _x, _y, _w, _h;
		const char                *_title;
		Test_view                 *_parent_view;

	public:

		Test_view(Nitpicker::Session_client *nitpicker,
		          int x, int y, int w, int h,
		          const char *title,
		          Test_view *parent_view = 0)
		:
			_nitpicker(*nitpicker),
			_x(x), _y(y), _w(w), _h(h), _title(title), _parent_view(parent_view)
		{
			using namespace Nitpicker;

			View_handle parent_handle;

			if (_parent_view)
				parent_handle = _nitpicker.view_handle(_parent_view->view_cap());

			_handle = _nitpicker.create_view(parent_handle);

			if (parent_handle.valid())
				_nitpicker.release_view_handle(parent_handle);

			Nitpicker::Rect rect(Nitpicker::Point(_x, _y), Nitpicker::Area(_w, _h));
			_nitpicker.enqueue<Command::Geometry>(_handle, rect);
			_nitpicker.enqueue<Command::To_front>(_handle, View_handle());
			_nitpicker.enqueue<Command::Title>(_handle, _title);
			_nitpicker.execute();
		}

		Nitpicker::View_capability view_cap()
		{
			return _nitpicker.view_capability(_handle);
		}

		void top()
		{
			_nitpicker.enqueue<Command::To_front>(_handle, View_handle());
			_nitpicker.execute();
		}

		/**
		 * Move view to absolute position
		 */
		void move(int x, int y)
		{
			/*
			 * If the view uses a parent view as corrdinate origin, we need to
			 * transform the absolute coordinates to parent-relative
			 * coordinates.
			 */
			_x = _parent_view ? x - _parent_view->x() : x;
			_y = _parent_view ? y - _parent_view->y() : y;

			Nitpicker::Rect rect(Nitpicker::Point(_x, _y), Nitpicker::Area(_w, _h));
			_nitpicker.enqueue<Command::Geometry>(_handle, rect);
			_nitpicker.execute();
		}

		/**
		 * Accessors
		 */
		const char *title() const { return _title; }
		int         x()     const { return _parent_view ? _parent_view->x() + _x : _x; }
		int         y()     const { return _parent_view ? _parent_view->y() + _y : _y; }
		int         w()     const { return _w; }
		int         h()     const { return _h; }
};


class Test_view_stack : public List<Test_view>
{
	private:

		unsigned char *_input_mask;
		unsigned _input_mask_w;
	public:

		Test_view_stack(unsigned char *input_mask, unsigned input_mask_w)
		: _input_mask(input_mask), _input_mask_w(input_mask_w) { }

		Test_view *find(int x, int y)
		{
			for (Test_view *tv = first(); tv; tv = tv->next()) {

				if (x < tv->x() || x >= tv->x() + tv->w()
				 || y < tv->y() || y >= tv->y() + tv->h())
					continue;

				if (!_input_mask)
					return tv;

				if (_input_mask[(y - tv->y())*_input_mask_w + (x - tv->x())])
					return tv;
			}

			return 0;
		}

		void top(Test_view *tv)
		{
			remove(tv);
			tv->top();
			insert(tv);
		}
};


void Component::construct(Genode::Env &env)
{
	/*
	 * Init sessions to the required external services
	 */
	enum { CONFIG_ALPHA = false };
	static Nitpicker::Connection nitpicker { env, "testnit" };
	static Timer::Connection     timer     { env };

	Framebuffer::Mode const mode(256, 256, Framebuffer::Mode::RGB565);
	nitpicker.buffer(mode, false);

	int const scr_w = mode.width(), scr_h = mode.height();

	log("screen is ", mode);
	if (!scr_w || !scr_h) {
		error("got invalid screen - sleeping forever");
		return;
	}

	Genode::Attached_dataspace fb_ds(
		env.rm(), nitpicker.framebuffer()->dataspace());

	short *pixels = fb_ds.local_addr<short>();
	unsigned char *alpha = (unsigned char *)&pixels[scr_w*scr_h];
	unsigned char *input_mask = CONFIG_ALPHA ? alpha + scr_w*scr_h : 0;

	/*
	 * Paint some crap into pixel buffer, fill alpha channel and input-mask buffer
	 *
	 * Input should refer to the view if the alpha value is more than 50%.
	 */
	for (int i = 0; i < scr_h; i++)
		for (int j = 0; j < scr_w; j++) {
			pixels[i*scr_w + j] = (i/8)*32*64 + (j/4)*32 + i*j/256;
			if (CONFIG_ALPHA) {
				alpha[i*scr_w + j] = (i*2) ^ (j*2);
				input_mask[i*scr_w + j] = alpha[i*scr_w + j] > 127;
			}
		}

	/*
	 * Create views to display the buffer
	 */
	Test_view_stack tvs(input_mask, scr_w);

	{
		/*
		 * View 'v1' is used as coordinate origin of 'v2' and 'v3'.
		 */
		static Test_view v1(&nitpicker, 150, 100, 230, 200, "Eins");
		static Test_view v2(&nitpicker,  20,  20, 230, 210, "Zwei", &v1);
		static Test_view v3(&nitpicker,  40,  40, 230, 220, "Drei", &v1);

		tvs.insert(&v1);
		tvs.insert(&v2);
		tvs.insert(&v3);
	}

	/*
	 * Handle input events
	 */
	int mx = 0, my = 0, key_cnt = 0;
	Test_view *tv = nullptr;
	while (1) {

		while (!nitpicker.input()->pending()) timer.msleep(20);

		nitpicker.input()->for_each_event([&] (Input::Event const &ev) {

			if (ev.press())   key_cnt++;
			if (ev.release()) key_cnt--;

			ev.handle_absolute_motion([&] (int x, int y) {

				/* move selected view */
				if (key_cnt > 0 && tv)
					tv->move(tv->x() + x - mx, tv->y() + y - my);

				mx = x; my = y;
			});

			/* find selected view and bring it to front */
			if (ev.press() && key_cnt == 1) {
				tv = tvs.find(mx, my);
				if (tv)
					tvs.top(tv);
			}
		});
	}

	env.parent().exit(0);
}
