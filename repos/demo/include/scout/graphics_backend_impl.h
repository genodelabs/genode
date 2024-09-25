/*
 * \brief   Graphics backend based on the GUI session interface
 * \date    2013-12-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__GRAPHICS_BACKEND_IMPL_H_
#define _INCLUDE__SCOUT__GRAPHICS_BACKEND_IMPL_H_

/* Genode includes */
#include <gui_session/connection.h>
#include <os/pixel_rgb888.h>
#include <base/attached_dataspace.h>

/* Scout includes */
#include <scout/graphics_backend.h>


namespace Scout {
	using Genode::Pixel_rgb888;
	class Graphics_backend_impl;
}


class Scout::Graphics_backend_impl : public Graphics_backend
{
	private:

		/*
		 * Noncopyable
		 */
		Graphics_backend_impl(Graphics_backend_impl const &);
		Graphics_backend_impl &operator = (Graphics_backend_impl const &);

		Genode::Region_map &_local_rm;

		Gui::Connection &_gui;

		Genode::Dataspace_capability _init_fb_ds(Area max_size)
		{
			_gui.buffer({ .area = { max_size.w, max_size.h*2 }, .alpha = false });

			return _gui.framebuffer.dataspace();
		}

		Area _max_size;

		Genode::Attached_dataspace _fb_ds { _local_rm, _init_fb_ds(_max_size) };

		Point        _position;
		Area         _view_size;
		bool         _flip_state = { false };

		Gui::Top_level_view _view { _gui, { _position, _view_size } };

		template <typename PT>
		PT *_base(unsigned idx)
		{
			return (PT *)_fb_ds.local_addr<PT>() + idx*_max_size.count();
		}

		using PT = Genode::Pixel_rgb888;

		Canvas<PT> _canvas_0, _canvas_1;

		Canvas_base *_canvas[2] { &_canvas_0, &_canvas_1 };

		unsigned _front_idx() const { return _flip_state ? 1 : 0; }
		unsigned  _back_idx() const { return _flip_state ? 0 : 1; }

	public:

		/**
		 * Constructor
		 *
		 * \param alloc  allocator used for allocating textures
		 */
		Graphics_backend_impl(Genode::Region_map &local_rm,
		                      Gui::Connection &gui,
		                      Genode::Allocator &alloc,
		                      Area max_size, Point position, Area view_size)
		:
			_local_rm(local_rm),
			_gui(gui),
			_max_size(max_size),
			_position(position),
			_view_size(view_size),
			_canvas_0(_base<PT>(0), max_size, alloc),
			_canvas_1(_base<PT>(1), max_size, alloc)
		{ }


		/********************************
		 ** Graphics_backend interface **
		 ********************************/

		Canvas_base &front() override { return *_canvas[_front_idx()]; }
		Canvas_base &back()  override { return *_canvas[ _back_idx()]; }

		void copy_back_to_front(Rect rect) override
		{
			Point const from = rect.p1() + Point { 0, int( _back_idx()*_max_size.h) };
			Point const to   = rect.p1() + Point { 0, int(_front_idx()*_max_size.h) };

			_gui.framebuffer.blit({ from, rect.area }, to);
		}

		void swap_back_and_front() override
		{
			_flip_state = !_flip_state;
			_gui.framebuffer.panning({ 0, int(_front_idx()*_max_size.h) });
		}

		void position(Point p) override
		{
			_position = p;
			_view.at(p);
		}

		void bring_to_front() override
		{
			_view.front();
		}

		void view_area(Area area) override
		{
			_view_size = area;
			_view.area(area);
		}
};

#endif /* _INCLUDE__SCOUT__GRAPHICS_BACKEND_IMPL_H_ */
