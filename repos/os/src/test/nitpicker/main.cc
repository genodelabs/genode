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
#include <gui_session/connection.h>
#include <input/event.h>
#include <os/pixel_rgb888.h>

namespace Test {

	using namespace Genode;

	class  View;
	struct Top_level_view;
	struct Child_view;
	class  View_stack;
	struct Main;
}


class Test::View : private List<View>::Element, Interface
{
	public:

		using Title = String<32>;

		struct Attr
		{
			Gui::Point pos;
			Gui::Area  size;
			Title      title;
		};

	private:

		friend class View_stack;
		friend class List<View>;

		using View_handle = Gui::Session::View_handle;
		using Command     = Gui::Session::Command;

		Gui::Connection  &_gui;
		View_handle const _handle;
		Attr        const _attr;
		Gui::Point        _pos = _attr.pos;

	public:

		View(Gui::Connection &gui, Attr const attr, auto const &create_fn)
		:
			_gui(gui), _handle(create_fn()), _attr(attr)
		{
			using namespace Gui;

			_gui.enqueue<Command::Geometry>(_handle, Gui::Rect { _pos, _attr.size });
			_gui.enqueue<Command::Front>(_handle);
			_gui.enqueue<Command::Title>(_handle, _attr.title);
			_gui.execute();
		}

		Gui::View_capability view_cap()
		{
			return _gui.view_capability(_handle);
		}

		void top()
		{
			_gui.enqueue<Command::Front>(_handle);
			_gui.execute();
		}

		virtual void move(Gui::Point const pos)
		{
			_pos = pos;
			_gui.enqueue<Command::Geometry>(_handle, Gui::Rect { _pos, _attr.size });
			_gui.execute();
		}

		virtual Gui::Point pos() const { return _pos; }

		Gui::Rect rect() const { return { pos(), _attr.size }; }

		bool contains(Gui::Point pos) { return rect().contains(pos); }
};


struct Test::Top_level_view : View
{
	Top_level_view(Gui::Connection &gui, Attr const &attr)
	:
		View(gui, attr, [&] { return gui.create_view(); })
	{ }
};


struct Test::Child_view : View
{
	View &_parent;

	Child_view(Gui::Connection &gui, View &parent, Attr const &attr)
	:
		View(gui, attr,
			[&] /* create_fn */ {
				using View_handle = Gui::Session::View_handle;
				View_handle const parent_handle = gui.alloc_view_handle(parent.view_cap());
				View_handle const handle = gui.create_child_view(parent_handle);
				gui.release_view_handle(parent_handle);
				return handle;
			}
		), _parent(parent)
	{ }

	void move(Gui::Point const pos) override
	{
		View::move(pos - _parent.pos());
	}

	Gui::Point pos() const override
	{
		return _parent.pos() + View::pos();
	}
};


class Test::View_stack : Noncopyable
{
	public:

		struct Input_mask_ptr
		{
			Gui::Area size;

			uint8_t *ptr;

			/**
			 * Return true if input at given position is enabled
			 */
			bool hit(Gui::Point const at) const
			{
				if (!ptr)
					return true;

				return Rect(Point(0, 0), size).contains(at)
				    && ptr[size.w*at.y + at.x];
			}
		};

	private:

		Input_mask_ptr const _input_mask_ptr;

		List<View> _views { };

		struct { View const *_dragged = nullptr; };

	public:

		View_stack(Input_mask_ptr input_mask) : _input_mask_ptr(input_mask) { }

		void with_view_at(Gui::Point const pos, auto const &fn)
		{
			View *tv = _views.first();
			for ( ; tv; tv = tv->next()) {

				if (!tv->contains(pos))
					continue;

				Point const rel = pos - tv->pos();

				if (_input_mask_ptr.hit(rel))
					break;
			}
			if (tv)
				fn(*tv);
		}

		void with_dragged_view(auto const &fn)
		{
			for (View *tv = _views.first(); tv; tv = tv->next())
				if (_dragged == tv)
					fn(*tv);
		}

		void insert(View &tv) { _views.insert(&tv); }

		void top(View &tv)
		{
			_views.remove(&tv);
			tv.top();
			_views.insert(&tv);
		}

		void drag(View const &tv) { _dragged = &tv; };

		void release_dragged_view() { _dragged = nullptr; }
};


struct Test::Main
{
	Env &_env;

	struct Config { bool alpha; } _config { .alpha = false };

	Gui::Connection _gui { _env, "testnit" };

	Constructible<Attached_dataspace> _fb_ds { };

	Constructible<View_stack> _view_stack { };

	/*
	 * View '_v1' is used as coordinate origin of '_v2' and '_v3'.
	 */
	Top_level_view _v1 { _gui,  { .pos   = { 150, 100 },
	                              .size  = { 230, 200 },
	                              .title = "Eins" } };

	Child_view _v2 { _gui, _v1, { .pos   = {  20,  20 },
	                              .size  = { 230, 210 },
	                              .title = "Zwei" } };

	Child_view _v3 { _gui, _v1, { .pos   = { 40,  40 },
	                              .size  = { 230, 220 },
	                              .title = "Drei" } };

	Signal_handler<Main> _input_handler { _env.ep(), *this, &Main::_handle_input };

	int _mx = 0, _my = 0, _key_cnt = 0;

	void _handle_input();

	Main(Env &env);
};


Test::Main::Main(Genode::Env &env) : _env(env)
{
	_gui.input.sigh(_input_handler);

	Gui::Area const size { 256, 256 };

	Framebuffer::Mode const mode { .area = size };

	log("screen is ", mode);

	_gui.buffer(mode, _config.alpha);

	_fb_ds.construct(_env.rm(), _gui.framebuffer.dataspace());

	/*
	 * Paint into pixel buffer, fill alpha channel and input-mask buffer
	 *
	 * Input should refer to the view if the alpha value is more than 50%.
	 */

	using PT = Pixel_rgb888;
	PT * const pixels = _fb_ds->local_addr<PT>();

	uint8_t * const alpha      = (uint8_t *)&pixels[size.count()];
	uint8_t * const input_mask = _config.alpha ? alpha + size.count() : 0;

	for (unsigned i = 0; i < size.h; i++)
		for (unsigned j = 0; j < size.w; j++) {
			pixels[i*size.w + j] = PT((3*i)/8, j, i*j/32);
			if (_config.alpha) {
				alpha[i*size.w + j] = (uint8_t)((i*2) ^ (j*2));
				input_mask[i*size.w + j] = alpha[i*size.w + j] > 127;
			}
		}

	_view_stack.construct(View_stack::Input_mask_ptr { .size = size,
	                                                   .ptr  = input_mask });

	_view_stack->insert(_v1);
	_view_stack->insert(_v2);
	_view_stack->insert(_v3);
}


void Test::Main::_handle_input()
{
	while (_gui.input.pending()) {

		_gui.input.for_each_event([&] (Input::Event const &ev) {

			if (ev.press())   _key_cnt++;
			if (ev.release()) _key_cnt--;

			ev.handle_absolute_motion([&] (int x, int y) {

				/* move selected view */
				_view_stack->with_dragged_view([&] (View &tv) {
					tv.move(tv.pos() + Point(x, y) - Point(_mx, _my)); });

				_mx = x; _my = y;
			});

			/* find selected view and bring it to front */
			if (ev.press() && _key_cnt == 1) {
				_view_stack->with_view_at(Point(_mx, _my), [&] (View &tv) {
					_view_stack->top(tv);
					_view_stack->drag(tv);
				});
			}

			if (ev.release() && _key_cnt == 0)
				_view_stack->release_dragged_view();
		});
	}
}


void Component::construct(Genode::Env &env) { static Test::Main main { env }; }
