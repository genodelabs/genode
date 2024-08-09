/*
 * \brief  Simple framework for rendering an animated scene
 * \author Norman Feske
 * \date   2015-06-22
 *
 * The 'Scene' class template contains the code for setting up a GUI
 * view with a triple-buffer for rendering tearing-free animations.
 * A derrived class implements the to-be-displayed content in the virtual
 * 'render' method.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NANO3D__SCENE_H_
#define _INCLUDE__NANO3D__SCENE_H_

/* Genode includes */
#include <base/entrypoint.h>
#include <timer_session/connection.h>
#include <gui_session/connection.h>
#include <os/surface.h>
#include <os/pixel_alpha8.h>
#include <base/attached_dataspace.h>
#include <input/event.h>

namespace Nano3d {

	struct Input_handler;
	template <typename> class Scene;
}


struct Nano3d::Input_handler : Genode::Interface
{
	virtual void handle_input(Input::Event const [], unsigned num_events) = 0;
};


template <typename PT>
class Nano3d::Scene
{
	public:

		class Unsupported_color_depth { };

		using Pixel_alpha8 = Genode::Pixel_alpha8;

		virtual void render(Genode::Surface<PT>           &pixel_surface,
		                    Genode::Surface<Pixel_alpha8> &alpha_surface) = 0;

	private:

		/**
		 * Noncopyable
		 */
		Scene(Scene const &);
		Scene &operator = (Scene const &);

		Genode::Env &_env;

		/**
		 * Position and size of GUI view
		 */
		Gui::Point const _pos;
		Gui::Area  const _size;

		Gui::Connection _gui { _env };

		struct Mapped_framebuffer
		{
			enum { NUM_BUFFERS = 3 };

			Genode::Region_map &rm;

			static Framebuffer::Session &
			_init_framebuffer(Gui::Connection &gui,
			                  Gui::Area const size)
			{
				/*
				 * Dimension the virtual framebuffer 3 times as high as the
				 * visible view because it contains the visible buffer, the
				 * front buffer, and the back buffer.
				 */
				bool     const use_alpha = true;
				unsigned const height    = size.h*NUM_BUFFERS;
				gui.buffer(Framebuffer::Mode { .area = { size.w, height } },
				                 use_alpha);

				return gui.framebuffer;
			}

			Framebuffer::Session &framebuffer;

			Framebuffer::Mode const mode = framebuffer.mode();

			/**
			 * Return visible size
			 */
			Gui::Area size() const
			{
				return Gui::Area(mode.area.w, mode.area.h/NUM_BUFFERS);
			}

			Genode::Attached_dataspace ds { rm, framebuffer.dataspace() };

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

			Mapped_framebuffer(Gui::Connection &gui, Gui::Area size,
			                   Genode::Region_map &rm)
			:
				rm(rm), framebuffer(_init_framebuffer(gui, size))
			{ }

		} _framebuffer { _gui, _size, _env.rm() };

		Gui::Top_level_view _view { _gui, { _pos, _size } };

		using Pixel_surface = Genode::Surface<PT>;
		using Alpha_surface = Genode::Surface<Genode::Pixel_alpha8>;

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
				Genode::size_t n = surface.size().count();
				for (T *dst = surface.addr(); n--; dst++)
					*dst = { };
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

		Timer::Connection _timer { _env };

		Genode::Attached_dataspace _input_ds { _env.rm(), _gui.input.dataspace() };

		Input_handler *_input_handler_callback = nullptr;

		void _handle_input()
		{
			if (!_input_handler_callback)
				return;

			while (int num = _gui.input.flush()) {

				auto const *ev_buf = _input_ds.local_addr<Input::Event>();

				if (_input_handler_callback)
					_input_handler_callback->handle_input(ev_buf, num);
			}
		}

		Genode::Signal_handler<Scene> _input_handler {
			_env.ep(), *this, &Scene::_handle_input };

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

		void _handle_period()
		{
			if (_do_sync)
				return;

			_surface_back->clear();

			render(_surface_back->pixel, _surface_back->alpha);

			_swap_back_and_front_surfaces();

			/* swap front and back buffers on next sync */
			_do_sync = true;
		}

		Genode::Signal_handler<Scene> _periodic_handler {
			_env.ep(), *this, &Scene::_handle_period };

		void _handle_sync()
		{
			/* rendering of scene is not complete, yet */
			if (!_do_sync)
				return;

			_swap_visible_and_front_surfaces();
			_swap_back_and_front_surfaces();

			int const h = _framebuffer.size().h;

			int const buf_y = (_surface_visible == &_surface_0) ? 0
			                : (_surface_visible == &_surface_1) ? -h
			                : -2*h;

			Gui::Point const offset(0, buf_y);
			_gui.enqueue<Command::Offset>(_view.id(), offset);
			_gui.execute();

			_do_sync = false;
		}

		Genode::Signal_handler<Scene> _sync_handler {
			_env.ep(), *this, &Scene::_handle_sync };

		using Command = Gui::Session::Command;

	public:

		Scene(Genode::Env &env, Genode::uint64_t update_rate_ms,
		      Gui::Point pos, Gui::Area size)
		:
			_env(env), _pos(pos), _size(size)
		{
			_gui.input.sigh(_input_handler);

			_timer.sigh(_periodic_handler);
			_timer.trigger_periodic(1000*update_rate_ms);

			_framebuffer.framebuffer.sync_sigh(_sync_handler);
		}

		virtual ~Scene() { }

		Genode::uint64_t elapsed_ms() const { return _timer.elapsed_ms(); }

		void input_handler(Input_handler *input_handler)
		{
			_framebuffer.input_mask(input_handler ? true : false);
			_input_handler_callback = input_handler;
		}
};

#endif /* _INCLUDE__NANO3D__SCENE_H_ */
