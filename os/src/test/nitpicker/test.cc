/*
 * \brief  Nitpicker test program
 * \author Norman Feske
 * \date   2006-08-23
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <util/list.h>
#include <base/sleep.h>
#include <base/printf.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_view/client.h>
#include <timer_session/connection.h>
#include <input/event.h>

using namespace Genode;

class Test_view : public List<Test_view>::Element
{
	private:

		Nitpicker::View_capability  _cap;
		int                         _x, _y, _w, _h;
		const char                 *_title;

	public:

		Test_view(Nitpicker::Session *nitpicker,
		          int x, int y, int w, int h,
		          const char *title)
		:
			_x(x), _y(y), _w(w), _h(h), _title(title)
		{
			using namespace Nitpicker;

			_cap = nitpicker->create_view();
			View_client(_cap).viewport(_x, _y, _w, _h, 0, 0, true);
			View_client(_cap).stack(Nitpicker::View_capability(), true, true);
			View_client(_cap).title(_title);
		}

		void top()
		{
			Nitpicker::View_client(_cap).stack(Nitpicker::View_capability(), true, true);
		}

		void move(int x, int y)
		{
			_x = x;
			_y = y;
			Nitpicker::View_client(_cap).viewport(_x, _y, _w, _h, 0, 0, true);
		}

		/**
		 * Accessors
		 */
		const char *title() { return _title; }
		int         x()     { return _x; }
		int         y()     { return _y; }
		int         w()     { return _w; }
		int         h()     { return _h; }
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


int main(int argc, char **argv)
{
	/*
	 * Init sessions to the required external services
	 */
	enum { CONFIG_ALPHA = false };
	static Nitpicker::Connection nitpicker;
	static Timer::Connection     timer;

	Framebuffer::Mode const mode(256, 256, Framebuffer::Mode::RGB565);
	nitpicker.buffer(mode, false);

	int const scr_w = mode.width(), scr_h = mode.height();

	printf("screen is %dx%d\n", scr_w, scr_h);
	if (!scr_w || !scr_h) {
		PERR("Got invalid screen - spinning");
		sleep_forever();
	}

	short *pixels = env()->rm_session()->attach(nitpicker.framebuffer()->dataspace());
	unsigned char *alpha = (unsigned char *)&pixels[scr_w*scr_h];
	unsigned char *input_mask = CONFIG_ALPHA ? alpha + scr_w*scr_h : 0;

	Input::Event *ev_buf = (env()->rm_session()->attach(nitpicker.input()->dataspace()));

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

	tvs.insert(new (env()->heap()) Test_view(&nitpicker, 150, 100, 230, 200, "Eins"));
	tvs.insert(new (env()->heap()) Test_view(&nitpicker, 170, 120, 230, 210, "Zwei"));
	tvs.insert(new (env()->heap()) Test_view(&nitpicker, 190, 140, 230, 220, "Drei"));

	/*
	 * Handle input events
	 */
	int omx = 0, omy = 0, key_cnt = 0;
	Test_view *tv = 0;
	while (1) {

		while (!nitpicker.input()->is_pending()) timer.msleep(20);

		for (int i = 0, num_ev = nitpicker.input()->flush(); i < num_ev; i++) {

			Input::Event *ev = &ev_buf[i];

			if (ev->type() == Input::Event::PRESS)   key_cnt++;
			if (ev->type() == Input::Event::RELEASE) key_cnt--;

			/* move selected view */
			if (ev->type() == Input::Event::MOTION && key_cnt > 0 && tv)
				tv->move(tv->x() + ev->ax() - omx, tv->y() + ev->ay() - omy);

			/* find selected view and bring it to front */
			if (ev->type() == Input::Event::PRESS && key_cnt == 1) {

				tv = tvs.find(ev->ax(), ev->ay());

				if (tv)
					tvs.top(tv);
			}

			omx = ev->ax(); omy = ev->ay();
		}
	}

	return 0;
}
