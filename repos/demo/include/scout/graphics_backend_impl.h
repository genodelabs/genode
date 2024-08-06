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
			Framebuffer::Mode const mode { .area = { max_size.w, max_size.h*2 }};

			_gui.buffer(mode, false);

			return _gui.framebuffer.dataspace();
		}

		Area _max_size;

		Genode::Attached_dataspace _fb_ds { _local_rm, _init_fb_ds(_max_size) };

		using View_handle = Gui::Session::View_handle;

		Point        _position;
		Area         _view_size;
		View_handle  _view { _gui.create_view() };
		Canvas_base *_canvas[2];
		bool         _flip_state = { false };

		void _update_viewport()
		{
			using Command = Gui::Session::Command;

			Gui::Rect rect(_position, _view_size);
			_gui.enqueue<Command::Geometry>(_view, rect);

			Gui::Point offset(0, _flip_state ? -_max_size.h : 0);
			_gui.enqueue<Command::Offset>(_view, offset);

			_gui.execute();
		}

		void _refresh_view(Rect rect)
		{
			int const y_offset = _flip_state ? _max_size.h : 0;
			_gui.framebuffer.refresh(rect.x1(), rect.y1() + y_offset,
			                         rect.w(), rect.h());
		}

		template <typename PT>
		PT *_base(unsigned idx)
		{
			return (PT *)_fb_ds.local_addr<PT>() + idx*_max_size.count();
		}

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
			_view_size(view_size)
		{
			bring_to_front();

			using PT = Genode::Pixel_rgb888;
			static Canvas<PT> canvas_0(_base<PT>(0), max_size, alloc);
			static Canvas<PT> canvas_1(_base<PT>(1), max_size, alloc);

			_canvas[0] = &canvas_0;
			_canvas[1] = &canvas_1;
		}


		/********************************
		 ** Graphics_backend interface **
		 ********************************/

		Canvas_base &front() override { return *_canvas[_front_idx()]; }
		Canvas_base &back()  override { return *_canvas[ _back_idx()]; }

		void copy_back_to_front(Rect rect) override
		{

			using PT = Genode::Pixel_rgb888;

			PT const *src = _base<PT>( _back_idx());
			PT       *dst = _base<PT>(_front_idx());

			unsigned long const offset = rect.y1()*_max_size.w + rect.x1();

			src += offset;
			dst += offset;

			blit(src, (unsigned)sizeof(PT)*_max_size.w, dst,
			     (int)sizeof(PT)*_max_size.w,
			     (int)sizeof(PT)*rect.w(), rect.h());

			_refresh_view(rect);
		}

		void swap_back_and_front() override
		{
			_flip_state = !_flip_state;
			_update_viewport();
		}

		void position(Point p) override
		{
			_position = p;
			_update_viewport();
		}

		void bring_to_front() override
		{
			using Command     = Gui::Session::Command;
			using View_handle = Gui::Session::View_handle;
			_gui.enqueue<Command::To_front>(_view, View_handle());
			_gui.execute();
		}

		void view_area(Area area) override
		{
			_view_size = area;
		}
};

#endif /* _INCLUDE__SCOUT__GRAPHICS_BACKEND_IMPL_H_ */
