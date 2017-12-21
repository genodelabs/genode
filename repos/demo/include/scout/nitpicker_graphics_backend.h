/*
 * \brief   Nitpicker-based graphics backend for scout
 * \date    2013-12-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__NITPICKER_GRAPHICS_BACKEND_H_
#define _INCLUDE__SCOUT__NITPICKER_GRAPHICS_BACKEND_H_

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <os/pixel_rgb565.h>
#include <base/attached_dataspace.h>

/* Scout includes */
#include <scout/graphics_backend.h>


namespace Scout {
	using Genode::Pixel_rgb565;
	class Nitpicker_graphics_backend;
}


class Scout::Nitpicker_graphics_backend : public Graphics_backend
{
	private:

		/*
		 * Noncopyable
		 */
		Nitpicker_graphics_backend(Nitpicker_graphics_backend const &);
		Nitpicker_graphics_backend &operator = (Nitpicker_graphics_backend const &);

		Genode::Region_map &_local_rm;

		Nitpicker::Connection &_nitpicker;

		Genode::Dataspace_capability _init_fb_ds(Area max_size)
		{
			_nitpicker.buffer(Framebuffer::Mode(max_size.w(), max_size.h()*2,
			                                    Framebuffer::Mode::RGB565), false);
			return _nitpicker.framebuffer()->dataspace();
		}

		Area _max_size;

		Genode::Attached_dataspace _fb_ds { _local_rm, _init_fb_ds(_max_size) };

		typedef Nitpicker::Session::View_handle View_handle;

		Point        _position;
		Area         _view_size;
		View_handle  _view { _nitpicker.create_view() };
		Canvas_base *_canvas[2];
		bool         _flip_state = { false };

		void _update_viewport()
		{
			typedef Nitpicker::Session::Command Command;

			Nitpicker::Rect rect(_position, _view_size);
			_nitpicker.enqueue<Command::Geometry>(_view, rect);

			Nitpicker::Point offset(0, _flip_state ? -_max_size.h() : 0);
			_nitpicker.enqueue<Command::Offset>(_view, offset);

			_nitpicker.execute();
		}

		void _refresh_view(Rect rect)
		{
			int const y_offset = _flip_state ? _max_size.h() : 0;
			_nitpicker.framebuffer()->refresh(rect.x1(), rect.y1() + y_offset,
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
		Nitpicker_graphics_backend(Genode::Region_map &local_rm,
		                           Nitpicker::Connection &nitpicker,
		                           Genode::Allocator &alloc,
		                           Area max_size, Point position, Area view_size)
		:
			_local_rm(local_rm),
			_nitpicker(nitpicker),
			_max_size(max_size),
			_position(position),
			_view_size(view_size)
		{
			bring_to_front();

			typedef Genode::Pixel_rgb565 PT;
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

		void copy_back_to_front(Rect rect)
		{

			typedef Genode::Pixel_rgb565 PT;

			PT const *src = _base<PT>( _back_idx());
			PT       *dst = _base<PT>(_front_idx());

			unsigned long const offset = rect.y1()*_max_size.w() + rect.x1();

			src += offset;
			dst += offset;

			blit(src, sizeof(PT)*_max_size.w(),
			     dst, sizeof(PT)*_max_size.w(), sizeof(PT)*rect.w(), rect.h());

			_refresh_view(rect);
		}

		void swap_back_and_front()
		{
			_flip_state = !_flip_state;
			_update_viewport();
		}

		void position(Point p)
		{
			_position = p;
			_update_viewport();
		}

		void bring_to_front()
		{
			typedef Nitpicker::Session::Command     Command;
			typedef Nitpicker::Session::View_handle View_handle;
			_nitpicker.enqueue<Command::To_front>(_view, View_handle());
			_nitpicker.execute();
		}

		void view_area(Area area)
		{
			_view_size = area;
		}
};

#endif /* _INCLUDE__SCOUT__NITPICKER_GRAPHICS_BACKEND_H_ */
