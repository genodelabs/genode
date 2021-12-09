/*
 * \brief  Fader for a GUI client
 * \author Norman Feske
 * \date   2014-09-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <gui_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <os/texture.h>
#include <os/surface.h>
#include <os/pixel_rgb888.h>
#include <os/pixel_alpha8.h>
#include <os/static_root.h>
#include <util/reconstructible.h>
#include <nitpicker_gfx/texture_painter.h>
#include <util/lazy_value.h>
#include <timer_session/connection.h>

/* local includes */
#include <alpha_dither_painter.h>

namespace Gui_fader {

	class Main;
	class Src_buffer;
	class Dst_buffer;
	class Framebuffer_session_component;
	class Gui_session_component;

	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Rect  Rect;

	using Genode::size_t;
	using Genode::Xml_node;
	using Genode::Dataspace_capability;
	using Genode::Attached_ram_dataspace;
	using Genode::Texture;
	using Genode::Surface;
	using Genode::Reconstructible;
	using Genode::Constructible;

	typedef Genode::Pixel_rgb888 Pixel_rgb888;
	typedef Genode::Pixel_alpha8 Pixel_alpha8;
}


/**
 * Buffer handed out to our client as virtual framebuffer
 */
class Gui_fader::Src_buffer
{
	private:

		typedef Pixel_rgb888 Pixel;

		bool             const _use_alpha;
		Attached_ram_dataspace _ds;
		Texture<Pixel>         _texture;

		static size_t _needed_bytes(Area size)
		{
			/* account for alpha channel, input mask, and pixels */
			return size.count() * (1 + 1 + sizeof(Pixel));
		}

	public:

		/**
		 * Constructor
		 */
		Src_buffer(Genode::Env &env, Area size, bool use_alpha)
		:
			_use_alpha(use_alpha),
			_ds(env.ram(), env.rm(), _needed_bytes(size)),
			_texture(_ds.local_addr<Pixel>(),
			         _ds.local_addr<unsigned char>() + size.count()*sizeof(Pixel),
			         size)
		{ }

		Dataspace_capability dataspace() { return _ds.cap(); }

		Texture<Pixel> const &texture() const { return _texture; }

		bool use_alpha() const { return _use_alpha; }
};


class Gui_fader::Dst_buffer
{
	private:

		Genode::Attached_dataspace _ds;
		Area                       _size;

		Surface<Pixel_rgb888> _pixel_surface { _ds.local_addr<Pixel_rgb888>(), _size };

		Surface<Pixel_alpha8> _alpha_surface
		{
			_ds.local_addr<Pixel_alpha8>() + _size.count()*sizeof(Pixel_rgb888),
			_size
		};

	public:

		Dst_buffer(Genode::Env &env, Dataspace_capability ds_cap, Area size)
		:
			_ds(env.rm(), ds_cap), _size(size)
		{
			/* initialize input-mask buffer */
			unsigned char *input_mask_buffer = _ds.local_addr<unsigned char>()
			                                 + _size.count()*(1 + sizeof(Pixel_rgb888));

			Genode::memset(input_mask_buffer, 0xff, _size.count());
		}

		Surface<Pixel_rgb888> &pixel_surface() { return _pixel_surface; }
		Surface<Pixel_alpha8> &alpha_surface() { return _alpha_surface; }
};


class Gui_fader::Framebuffer_session_component
:
	public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		Genode::Env &_env;

		Gui::Connection &_gui;
		Src_buffer      &_src_buffer;

		Constructible<Dst_buffer> _dst_buffer { };


		Lazy_value<int> _fade { };

	public:

		/**
		 * Constructor
		 */
		Framebuffer_session_component(Genode::Env     &env,
		                              Gui::Connection &gui,
		                              Src_buffer      &src_buffer)
		:
			_env(env), _gui(gui), _src_buffer(src_buffer)
		{ }

		void dst_buffer(Dataspace_capability ds_cap, Area size)
		{
			_dst_buffer.construct(_env, ds_cap, size);
		}

		void transfer_src_to_dst_pixel(Rect const rect)
		{
			if (!_dst_buffer.constructed())
				return;

			_dst_buffer->pixel_surface().clip(rect);

			Texture_painter::paint(_dst_buffer->pixel_surface(),
			                       _src_buffer.texture(),
			                       Genode::Color(0, 0, 0),
			                       Point(0, 0),
			                       Texture_painter::SOLID,
			                       false);
		}

		void transfer_src_to_dst_alpha(Rect const rect)
		{
			if (!_dst_buffer.constructed())
				return;

			_dst_buffer->alpha_surface().clip(rect);

			if (_src_buffer.use_alpha()) {
				Alpha_dither_painter::paint(_dst_buffer->alpha_surface(), rect, _fade,
				                            _src_buffer.texture());
			} else {
				Alpha_dither_painter::paint(_dst_buffer->alpha_surface(), rect, _fade);
			}
		}

		Area size()
		{
			return _dst_buffer.constructed() ? _dst_buffer->pixel_surface().size()
			                                 : Area();
		}

		bool animate(unsigned num_frames)
		{
			for (unsigned i = 0; i < num_frames; i++)
				_fade.animate();

			Rect const rect(Point(0, 0), size());

			transfer_src_to_dst_alpha(rect);

			_gui.framebuffer()->refresh(rect.x1(), rect.y1(), rect.w(), rect.h());

			/* keep animating as long as the destination value is not reached */
			return _fade != _fade.dst();
		}

		void fade(int fade_value, int steps) { _fade.dst(fade_value, steps); }

		bool visible() const { return _fade != 0; }



		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override
		{
			return _src_buffer.dataspace();
		}

		Framebuffer::Mode mode() const override
		{
			return _gui.framebuffer()->mode();
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			_gui.framebuffer()->mode_sigh(sigh);
		}

		void refresh(int x, int y, int w, int h) override
		{
			transfer_src_to_dst_pixel(Rect(Point(x, y), Area(w, h)));
			transfer_src_to_dst_alpha(Rect(Point(x, y), Area(w, h)));

			_gui.framebuffer()->refresh(x, y, w, h);
		}

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_gui.framebuffer()->sync_sigh(sigh);
		}
};


class Gui_fader::Gui_session_component
:
	public Genode::Rpc_object<Gui::Session>
{
	private:

		typedef Gui::View_capability      View_capability;
		typedef Gui::Session::View_handle View_handle;

		Genode::Env &_env;

		Reconstructible<Src_buffer> _src_buffer { _env, Area(1, 1), false };

		Gui::Connection _gui { _env };

		Genode::Attached_ram_dataspace _command_ds {
			_env.ram(), _env.rm(), sizeof(Gui::Session::Command_buffer) };

		Gui::Session::Command_buffer &_commands =
			*_command_ds.local_addr<Gui::Session::Command_buffer>();

		Framebuffer_session_component _fb_session { _env, _gui, *_src_buffer };

		Framebuffer::Session_capability _fb_cap { _env.ep().manage(_fb_session) };

		Gui::Session::View_handle _view_handle { };

		bool _view_visible = false;
		Rect _view_geometry { };

		void _update_view_visibility()
		{
			if (!_view_handle.valid() || (_view_visible == _fb_session.visible()))
				return;

			typedef Gui::Session::Command Command;

			if (_fb_session.visible())
				_gui.enqueue<Command::Geometry>(_view_handle, _view_geometry);
			else
				_gui.enqueue<Command::Geometry>(_view_handle, Rect());

			_gui.execute();

			_view_visible = _fb_session.visible();
		}

	public:

		/**
		 * Constructor
		 */
		Gui_session_component(Genode::Env &env) : _env(env)
		{ }

		/**
		 * Destructor
		 */
		~Gui_session_component()
		{
			_env.ep().dissolve(_fb_session);
		}

		bool animate(unsigned num_frames)
		{
			bool const keep_animating =  _fb_session.animate(num_frames);

			_update_view_visibility();

			return keep_animating;
		}

		void fade(int fade_value, int steps)
		{
			_fb_session.fade(fade_value, steps);
		}


		/****************************
		 ** Gui::Session interface **
		 ****************************/

		Framebuffer::Session_capability framebuffer_session() override
		{
			return _fb_cap;
		}

		Input::Session_capability input_session() override
		{
			return _gui.input_session();
		}

		View_handle create_view(View_handle parent) override
		{
			_view_handle = _gui.create_view(parent);
			_update_view_visibility();
			return _view_handle;
		}

		void destroy_view(View_handle handle) override
		{
			return _gui.destroy_view(handle);
		}

		View_handle view_handle(View_capability view_cap,
		                        View_handle handle) override
		{
			return _gui.view_handle(view_cap, handle);
		}

		View_capability view_capability(View_handle handle) override
		{
			return _gui.view_capability(handle);
		}

		void release_view_handle(View_handle handle) override
		{
			_gui.release_view_handle(handle);
		}

		Dataspace_capability command_dataspace() override
		{
			return _command_ds.cap();
		}

		void execute() override
		{
			for (unsigned i = 0; i < _commands.num(); i++) {

				Gui::Session::Command command = _commands.get(i);

				bool forward_command = true;

				if (command.opcode == Gui::Session::Command::OP_GEOMETRY) {

					/* remember view geometry as defined by the client */
					_view_geometry = command.geometry.rect;

					if (!_view_visible)
						forward_command = false;
				}

				if (forward_command)
					_gui.enqueue(command);
			}
			_fb_session.transfer_src_to_dst_pixel(Rect(Point(0, 0), _fb_session.size()));
			_fb_session.transfer_src_to_dst_alpha(Rect(Point(0, 0), _fb_session.size()));
			return _gui.execute();
		}

		Framebuffer::Mode mode() override
		{
			return _gui.mode();
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			_gui.mode_sigh(sigh);
		}

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			Area const size = mode.area;

			_src_buffer.construct(_env, size, use_alpha);

			_gui.buffer(mode, true);

			_fb_session.dst_buffer(_gui.framebuffer()->dataspace(), size);
		}

		void focus(Genode::Capability<Session> focused) override
		{
			_gui.focus(focused);
		}
};


struct Gui_fader::Main
{
	Genode::Env &env;

	Genode::Attached_rom_dataspace config { env, "config" };

	Timer::Connection timer { env };

	enum { PERIOD = 20 };

	unsigned alpha = 0;

	unsigned fade_in_steps  = 0;
	unsigned fade_out_steps = 0;

	bool initial_fade_in = true;

	unsigned initial_fade_in_steps  = 0;

	Genode::uint64_t curr_frame() const { return timer.elapsed_ms() / PERIOD; }

	Genode::uint64_t last_frame = 0;

	void handle_config_update();

	Genode::Signal_handler<Main> config_handler
	{
		env.ep(), *this, &Main::handle_config_update
	};

	Gui_session_component gui_session { env };

	Genode::Static_root<Gui::Session> gui_root
	{
		env.ep().manage(gui_session)
	};

	void handle_timer()
	{
		Genode::uint64_t frame = curr_frame();
		if (gui_session.animate((unsigned)(frame - last_frame)))
			timer.trigger_once(PERIOD);

		last_frame = frame;
	}

	Genode::Signal_handler<Main> timer_handler
	{
		env.ep(), *this, &Main::handle_timer
	};

	Main(Genode::Env &env) : env(env)
	{
		config.sigh(config_handler);

		timer.sigh(timer_handler);

		/* apply initial config */
		handle_config_update();

		env.parent().announce(env.ep().manage(gui_root));
	}
};


void Gui_fader::Main::handle_config_update()
{
	config.update();

	Genode::Xml_node config_xml = config.xml();

	unsigned new_alpha = alpha;
	if (config_xml.has_attribute("alpha"))
		config_xml.attribute("alpha").value(new_alpha);

	fade_in_steps         = config_xml.attribute_value("fade_in_steps",  20U);
	fade_out_steps        = config_xml.attribute_value("fade_out_steps", 50U);
	initial_fade_in_steps = config_xml.attribute_value("initial_fade_in_steps", fade_in_steps);

	/* respond to state change */
	if (new_alpha != alpha) {

		bool const fade_in = (new_alpha > alpha);

		unsigned const steps =
			fade_in ? (initial_fade_in ? initial_fade_in_steps : fade_in_steps)
			        : fade_out_steps;

		initial_fade_in = false;

		gui_session.fade(280*new_alpha, steps);

		alpha = new_alpha;

		/* schedule animation */
		last_frame = curr_frame();
		timer.trigger_once(PERIOD);
	}
}


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) {
		static Gui_fader::Main desktop(env); }
