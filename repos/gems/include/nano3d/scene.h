/*
 * \brief  Simple framework for rendering an animated scene
 * \author Norman Feske
 * \date   2015-06-22
 *
 * The 'Scene' class template contains the code for setting up a nitpicker
 * view with a triple-buffer for rendering tearing-free animations.
 * A derrived class implements the to-be-displayed content in the virtual
 * 'render' method.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NANO3D__SCENE_H_
#define _INCLUDE__NANO3D__SCENE_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <nitpicker_session/connection.h>
#include <os/surface.h>
#include <os/pixel_alpha8.h>
#include <os/attached_dataspace.h>
#include <input/event.h>

namespace Nano3d {

	struct Input_handler;
	template <typename> class Scene;
}


struct Nano3d::Input_handler
{
	virtual void handle_input(Input::Event const [], unsigned num_events) = 0;
};


template <typename PT>
class Nano3d::Scene
{
	public:

		class Unsupported_color_depth { };

		typedef Genode::Pixel_alpha8 Pixel_alpha8;

		virtual void render(Genode::Surface<PT>           &pixel_surface,
		                    Genode::Surface<Pixel_alpha8> &alpha_surface) = 0;

	private:

		Genode::Signal_receiver &_sig_rec;

		/**
		 * Position and size of nitpicker view
		 */
		Nitpicker::Point const _pos;
		Nitpicker::Area  const _size;

		Nitpicker::Connection _nitpicker;

		struct Mapped_framebuffer
		{
			enum { NUM_BUFFERS = 3 };

			static Framebuffer::Session &
			_init_framebuffer(Nitpicker::Connection &nitpicker,
			                  Nitpicker::Area const size)
			{
				Framebuffer::Mode::Format const format = nitpicker.mode().format();
				if (format != Framebuffer::Mode::RGB565) {
					Genode::error("framebuffer mode ", (int)format, " is not supported");
					throw Unsupported_color_depth();
				}

				/*
				 * Dimension the virtual framebuffer 3 times as high as the
				 * visible view because it contains the visible buffer, the
				 * front buffer, and the back buffer.
				 */
				bool     const use_alpha = true;
				unsigned const height    = size.h()*NUM_BUFFERS;
				nitpicker.buffer(Framebuffer::Mode(size.w(), height, format),
				                 use_alpha);

				return *nitpicker.framebuffer();
			}

			Framebuffer::Session &framebuffer;

			Framebuffer::Mode const mode = framebuffer.mode();

			/**
			 * Return visible size
			 */
			Nitpicker::Area size() const
			{
				return Nitpicker::Area(mode.width(), mode.height()/NUM_BUFFERS);
			}

			Genode::Attached_dataspace ds { framebuffer.dataspace() };

			PT *pixel_base(unsigned i)
			{
				return (PT *)(ds.local_addr<PT>() + i*size().count());
			}

			Pixel_alpha8 *alpha_base(unsigned i)
			{
				Pixel_alpha8 * const alpha_base =
					(Pixel_alpha8 *)(ds.local_addr<PT>() + NUM_BUFFERS*size().count());

				return alpha_base + i*size().count();
			}

			/**
			 * Set or clear the input mask for the virtual framebuffer
			 */
			void input_mask(bool input_enabled)
			{
				/*
				 * The input-mask buffer follows the alpha buffer. Hence, we
				 * can obtain the base address by requesting the base of
				 * the (non-exiting) alpha buffer (using NUM_BUFFERS as index)
				 * beyond the actual alpha buffers.
				 */
				Genode::memset(alpha_base(NUM_BUFFERS), input_enabled,
				               NUM_BUFFERS*size().count());
			}

			Mapped_framebuffer(Nitpicker::Connection &nitpicker, Nitpicker::Area size)
			:
				framebuffer(_init_framebuffer(nitpicker, size))
			{ }

		} _framebuffer { _nitpicker, _size };

		Nitpicker::Session::View_handle _view_handle = _nitpicker.create_view();

		typedef Genode::Surface<PT>                   Pixel_surface;
		typedef Genode::Surface<Genode::Pixel_alpha8> Alpha_surface;

		struct Surface
		{
			Pixel_surface pixel;
			Alpha_surface alpha;

			Surface(PT *pixel_base, Genode::Pixel_alpha8 *alpha_base,
			        Genode::Surface_base::Area size)
			:
				pixel(pixel_base, size), alpha(alpha_base, size)
			{ }

			Genode::Surface_base::Area size() const { return pixel.size(); }

			template <typename T>
			void _clear(Genode::Surface<T> &surface)
			{
				Genode::size_t n = (surface.size().count()*sizeof(T))/sizeof(long);
				for (long *dst = (long *)surface.addr(); n--; dst++)
					*dst = 0;
			}

			void clear()
			{
				_clear(pixel);
				_clear(alpha);
			}
		};

		Surface _surface_0 { _framebuffer.pixel_base(0), _framebuffer.alpha_base(0),
		                     _framebuffer.size() };
		Surface _surface_1 { _framebuffer.pixel_base(1), _framebuffer.alpha_base(1),
		                     _framebuffer.size() };
		Surface _surface_2 { _framebuffer.pixel_base(2), _framebuffer.alpha_base(2),
		                     _framebuffer.size() };

		Surface *_surface_visible = &_surface_0;
		Surface *_surface_front   = &_surface_1;
		Surface *_surface_back    = &_surface_2;

		bool _do_sync = false;

		Timer::Connection _timer;

		Genode::Attached_dataspace _input_ds { _nitpicker.input()->dataspace() };

		Input_handler *_input_handler = nullptr;

		void _handle_input(unsigned)
		{
			if (!_input_handler)
				return;

			while (int num = _nitpicker.input()->flush()) {

				auto const *ev_buf = _input_ds.local_addr<Input::Event>();

				if (_input_handler)
					_input_handler->handle_input(ev_buf, num);
			}
		}

		Genode::Signal_dispatcher<Scene> _input_dispatcher {
			_sig_rec, *this, &Scene::_handle_input };

		void _swap_back_and_front_surfaces()
		{
			Surface *tmp   = _surface_back;
			_surface_back  = _surface_front;
			_surface_front = tmp;
		}

		void _swap_visible_and_front_surfaces()
		{
			Surface *tmp      = _surface_visible;
			_surface_visible  = _surface_front;
			_surface_front    = tmp;
		}

		void _handle_period(unsigned)
		{
			if (_do_sync)
				return;

			_surface_back->clear();

			render(_surface_back->pixel, _surface_back->alpha);

			_swap_back_and_front_surfaces();

			/* swap front and back buffers on next sync */
			_do_sync = true;
		}

		Genode::Signal_dispatcher<Scene> _periodic_dispatcher {
			_sig_rec, *this, &Scene::_handle_period };

		void _handle_sync(unsigned)
		{
			/* rendering of scene is not complete, yet */
			if (!_do_sync)
				return;

			_swap_visible_and_front_surfaces();
			_swap_back_and_front_surfaces();

			int const h = _framebuffer.size().h();

			int const buf_y = (_surface_visible == &_surface_0) ? 0
			                : (_surface_visible == &_surface_1) ? -h
			                : -2*h;

			Nitpicker::Point const offset(0, buf_y);
			_nitpicker.enqueue<Command::Offset>(_view_handle, offset);
			_nitpicker.execute();

			_do_sync = false;
		}

		Genode::Signal_dispatcher<Scene> _sync_dispatcher {
			_sig_rec, *this, &Scene::_handle_sync };

		typedef Nitpicker::Session::Command Command;

	public:

		Scene(Genode::Signal_receiver &sig_rec, unsigned update_rate_ms,
		      Nitpicker::Point pos, Nitpicker::Area size)
		:
			_sig_rec(sig_rec), _pos(pos), _size(size)
		{
			Nitpicker::Rect rect(_pos, _size);
			_nitpicker.enqueue<Command::Geometry>(_view_handle, rect);
			_nitpicker.enqueue<Command::To_front>(_view_handle);
			_nitpicker.execute();

			_nitpicker.input()->sigh(_input_dispatcher);

			_timer.sigh(_periodic_dispatcher);
			_timer.trigger_periodic(1000*update_rate_ms);

			_framebuffer.framebuffer.sync_sigh(_sync_dispatcher);
		}

		static void dispatch_signals_loop(Genode::Signal_receiver &sig_rec)
		{
			while (1) {

				Genode::Signal signal = sig_rec.wait_for_signal();

				Genode::Signal_dispatcher_base *dispatcher =
					static_cast<Genode::Signal_dispatcher_base *>(signal.context());

				dispatcher->dispatch(signal.num());
			}
		}

		unsigned long elapsed_ms() const { return _timer.elapsed_ms(); }

		void input_handler(Input_handler *input_handler)
		{
			_framebuffer.input_mask(input_handler ? true : false);
			_input_handler = input_handler;
		}
};

#endif /* _INCLUDE__NANO3D__SCENE_H_ */
