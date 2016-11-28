/*
 * \brief  Fader for a nitpicker client
 * \author Norman Feske
 * \date   2014-09-08
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <os/config.h>
#include <base/printf.h>
#include <nitpicker_session/connection.h>
#include <os/attached_ram_dataspace.h>
#include <os/texture.h>
#include <os/surface.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_alpha8.h>
#include <os/static_root.h>
#include <util/volatile_object.h>
#include <nitpicker_gfx/texture_painter.h>
#include <util/lazy_value.h>
#include <timer_session/connection.h>

/* local includes */
#include <alpha_dither_painter.h>

namespace Nit_fader {

	class Main;
	class Src_buffer;
	class Dst_buffer;
	class Framebuffer_session_component;
	class Nitpicker_session_component;

	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Rect  Rect;

	using Genode::size_t;
	using Genode::env;
	using Genode::Xml_node;
	using Genode::Dataspace_capability;
	using Genode::Attached_ram_dataspace;
	using Genode::Texture;
	using Genode::Surface;
	using Genode::Volatile_object;
	using Genode::Lazy_volatile_object;

	typedef Genode::Pixel_rgb565 Pixel_rgb565;
	typedef Genode::Pixel_alpha8 Pixel_alpha8;
}


/**
 * Buffer handed out to our client as virtual framebuffer
 */
class Nit_fader::Src_buffer
{
	private:

		typedef Pixel_rgb565 Pixel;

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
		Src_buffer(Area size, bool use_alpha)
		:
			_use_alpha(use_alpha),
			_ds(env()->ram_session(), _needed_bytes(size)),
			_texture(_ds.local_addr<Pixel>(),
			         _ds.local_addr<unsigned char>() + size.count()*sizeof(Pixel),
			         size)
		{ }

		Dataspace_capability dataspace() { return _ds.cap(); }

		Texture<Pixel> const &texture() const { return _texture; }

		bool use_alpha() const { return _use_alpha; }
};


class Nit_fader::Dst_buffer
{
	private:

		Genode::Attached_dataspace _ds;
		Area                       _size;

		Surface<Pixel_rgb565> _pixel_surface { _ds.local_addr<Pixel_rgb565>(), _size };

		Surface<Pixel_alpha8> _alpha_surface
		{
			_ds.local_addr<Pixel_alpha8>() + _size.count()*sizeof(Pixel_rgb565),
			_size
		};

	public:

		Dst_buffer(Dataspace_capability ds_cap, Area size)
		:
			_ds(ds_cap), _size(size)
		{
			/* initialize input-mask buffer */
			unsigned char *input_mask_buffer = _ds.local_addr<unsigned char>()
			                                 + _size.count()*(1 + sizeof(Pixel_rgb565));

			Genode::memset(input_mask_buffer, 0xff, _size.count());
		}

		Surface<Pixel_rgb565> &pixel_surface() { return _pixel_surface; }
		Surface<Pixel_alpha8> &alpha_surface() { return _alpha_surface; }
};


class Nit_fader::Framebuffer_session_component
:
	public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		Nitpicker::Connection &_nitpicker;
		Src_buffer            &_src_buffer;

		Lazy_volatile_object<Dst_buffer> _dst_buffer;


		Lazy_value<int> _fade;

	public:

		/**
		 * Constructor
		 */
		Framebuffer_session_component(Nitpicker::Connection &nitpicker,
		                              Src_buffer            &src_buffer)
		:
			_nitpicker(nitpicker), _src_buffer(src_buffer)
		{ }

		void dst_buffer(Dataspace_capability ds_cap, Area size)
		{
			_dst_buffer.construct(ds_cap, size);
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

			_nitpicker.framebuffer()->refresh(rect.x1(), rect.y1(), rect.w(), rect.h());

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
			return _nitpicker.framebuffer()->mode();
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			_nitpicker.framebuffer()->mode_sigh(sigh);
		}

		void refresh(int x, int y, int w, int h) override
		{
			transfer_src_to_dst_pixel(Rect(Point(x, y), Area(w, h)));
			transfer_src_to_dst_alpha(Rect(Point(x, y), Area(w, h)));

			_nitpicker.framebuffer()->refresh(x, y, w, h);
		}

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_nitpicker.framebuffer()->sync_sigh(sigh);
		}
};


class Nit_fader::Nitpicker_session_component
:
	public Genode::Rpc_object<Nitpicker::Session>
{
	private:

		typedef Nitpicker::View_capability      View_capability;
		typedef Nitpicker::Session::View_handle View_handle;

		Server::Entrypoint &_ep;

		Volatile_object<Src_buffer> _src_buffer { Area(1, 1), false };

		Nitpicker::Connection _nitpicker;

		Genode::Attached_ram_dataspace _command_ds {
			env()->ram_session(), sizeof(Nitpicker::Session::Command_buffer) };

		Nitpicker::Session::Command_buffer &_commands =
			*_command_ds.local_addr<Nitpicker::Session::Command_buffer>();

		Framebuffer_session_component _fb_session { _nitpicker, *_src_buffer };

		Framebuffer::Session_capability _fb_cap { _ep.manage(_fb_session) };

		Nitpicker::Session::View_handle _view_handle;
		bool _view_visible = false;
		Rect _view_geometry;

		void _update_view_visibility()
		{
			if (!_view_handle.valid() || (_view_visible == _fb_session.visible()))
				return;

			typedef Nitpicker::Session::Command Command;

			if (_fb_session.visible())
				_nitpicker.enqueue<Command::Geometry>(_view_handle, _view_geometry);
			else
				_nitpicker.enqueue<Command::Geometry>(_view_handle, Rect());

			_nitpicker.execute();

			_view_visible = _fb_session.visible();
		}

	public:

		/**
		 * Constructor
		 */
		Nitpicker_session_component(Server::Entrypoint &ep) : _ep(ep)
		{ }

		/**
		 * Destructor
		 */
		~Nitpicker_session_component()
		{
			_ep.dissolve(_fb_session);
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


		/**********************************
		 ** Nitpicker::Session interface **
		 **********************************/

		Framebuffer::Session_capability framebuffer_session() override
		{
			return _fb_cap;
		}

		Input::Session_capability input_session() override
		{
			return _nitpicker.input_session();
		}

		View_handle create_view(View_handle parent) override
		{
			_view_handle = _nitpicker.create_view(parent);
			return _view_handle;
		}

		void destroy_view(View_handle handle) override
		{
			return _nitpicker.destroy_view(handle);
		}

		View_handle view_handle(View_capability view_cap,
		                        View_handle handle) override
		{
			return _nitpicker.view_handle(view_cap, handle);
		}

		View_capability view_capability(View_handle handle) override
		{
			return _nitpicker.view_capability(handle);
		}

		void release_view_handle(View_handle handle) override
		{
			_nitpicker.release_view_handle(handle);
		}

		Dataspace_capability command_dataspace() override
		{
			return _command_ds.cap();
		}

		void execute() override
		{
			for (unsigned i = 0; i < _commands.num(); i++) {

				Nitpicker::Session::Command command = _commands.get(i);

				bool forward_command = true;

				if (command.opcode == Nitpicker::Session::Command::OP_GEOMETRY) {

					/* remember view geometry as defined by the client */
					_view_geometry = command.geometry.rect;

					if (!_view_visible)
						forward_command = false;
				}

				if (forward_command)
					_nitpicker.enqueue(command);
			}
			_fb_session.transfer_src_to_dst_pixel(Rect(Point(0, 0), _fb_session.size()));
			_fb_session.transfer_src_to_dst_alpha(Rect(Point(0, 0), _fb_session.size()));
			return _nitpicker.execute();
		}

		Framebuffer::Mode mode() override
		{
			return _nitpicker.mode();
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			_nitpicker.mode_sigh(sigh);
		}

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			Area const size(mode.width(), mode.height());

			_src_buffer.construct(size, use_alpha);

			_nitpicker.buffer(mode, true);

			_fb_session.dst_buffer(_nitpicker.framebuffer()->dataspace(), size);
		}

		void focus(Genode::Capability<Session> focused) override
		{
			_nitpicker.focus(focused);
		}
};


struct Nit_fader::Main
{
	Server::Entrypoint &ep;

	Timer::Connection timer;

	enum { PERIOD = 20 };

	unsigned long alpha = 0;

	unsigned long curr_frame() const { return timer.elapsed_ms() / PERIOD; }

	unsigned long last_frame = 0;

	void handle_config_update(unsigned);

	Genode::Signal_rpc_member<Main> config_dispatcher
	{
		ep, *this, &Main::handle_config_update
	};

	Nitpicker_session_component nitpicker_session { ep };

	Genode::Static_root<Nitpicker::Session> nitpicker_root
	{
		ep.manage(nitpicker_session)
	};

	void handle_timer(unsigned)
	{
		unsigned long frame = curr_frame();
		if (nitpicker_session.animate(frame - last_frame))
			timer.trigger_once(PERIOD);

		last_frame = frame;
	}

	Genode::Signal_rpc_member<Main> timer_dispatcher
	{
		ep, *this, &Main::handle_timer
	};

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		Genode::config()->sigh(config_dispatcher);

		timer.sigh(timer_dispatcher);

		/* apply initial config */
		handle_config_update(0);

		env()->parent()->announce(ep.manage(nitpicker_root));
	}
};


void Nit_fader::Main::handle_config_update(unsigned)
{
	Genode::config()->reload();

	Genode::Xml_node config_xml = Genode::config()->xml_node();

	unsigned long new_alpha = alpha;
	if (config_xml.has_attribute("alpha"))
		config_xml.attribute("alpha").value(&new_alpha);

	/* respond to state change */
	if (new_alpha != alpha) {

		int const steps = (new_alpha > alpha) ? 20 : 50;
		nitpicker_session.fade(280*new_alpha, steps);

		alpha = new_alpha;

		/* schedule animation */
		last_frame = curr_frame();
		timer.trigger_once(PERIOD);
	}
}


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "nit_fader_ep"; }

	size_t stack_size() { return 16*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Nit_fader::Main desktop(ep);
	}
}
