/*
 * \brief  Frame-buffer driver for the i.MX53
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-06-21
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <imx_framebuffer_session/imx_framebuffer_session.h>
#include <timer_session/connection.h>
#include <dataspace/client.h>
#include <os/static_root.h>
#include <blit/blit.h>

/* local includes */
#include <driver.h>

namespace Framebuffer {
	using namespace Genode;
	class Session_component;
};


class Framebuffer::Session_component :
	public Genode::Rpc_object<Framebuffer::Imx_session>
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		Genode::Env &_env;

		bool     _buffered;
		Mode     _mode;
		size_t   _size;

		/* dataspace uses a back buffer (if '_buffered' is true) */
		Genode::Dataspace_capability _bb_ds;
		void                        *_bb_addr;

		/* dataspace of physical frame buffer */
		Genode::Dataspace_capability _fb_ds;
		void                        *_fb_addr;

		Timer::Connection _timer { _env };


		Ipu &_ipu;

		void _refresh_buffered(int x, int y, int w, int h)
		{
			/* clip specified coordinates against screen boundaries */
			int x2 = min(x + w - 1, (int)_mode.width()  - 1),
				y2 = min(y + h - 1, (int)_mode.height() - 1);
			int x1 = max(x, 0),
				y1 = max(y, 0);
			if (x1 > x2 || y1 > y2) return;

			int bypp = _mode.bytes_per_pixel();

			/* copy pixels from back buffer to physical frame buffer */
			char *src = (char *)_bb_addr + bypp*(_mode.width()*y1 + x1),
			     *dst = (char *)_fb_addr + bypp*(_mode.width()*y1 + x1);

			blit(src, bypp*_mode.width(), dst, bypp*_mode.width(),
			     bypp*(x2 - x1 + 1), y2 - y1 + 1);
		}

	public:

		Session_component(Genode::Env &env, Driver &driver, bool buffered)
		: _env(env),
		  _buffered(buffered),
		  _mode(driver.mode()),
		  _size(_mode.bytes_per_pixel() * _mode.width() * _mode.height()),
		  _bb_ds(buffered ? _env.ram().alloc(_size)
		                  : Genode::Ram_dataspace_capability()),
		  _bb_addr(buffered ? (void*)_env.rm().attach(_bb_ds) : 0),
		  _fb_ds(_env.ram().alloc(_size, WRITE_COMBINED)),
		  _fb_addr((void*)_env.rm().attach(_fb_ds)),
		  _ipu(driver.ipu())
		{
			if (!driver.init(Dataspace_client(_fb_ds).phys_addr())) {
				error("could not initialize display");
				struct Could_not_initialize_display : Exception { };
				throw Could_not_initialize_display();
			}
		}


		/**************************************
		 **  Framebuffer::session interface  **
		 **************************************/

		Dataspace_capability dataspace() override { return _buffered ? _bb_ds : _fb_ds; }
		Mode mode() const override { return _mode; }
		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override
		{
			if (_buffered)
				_refresh_buffered(x, y, w, h);
		}

		void overlay(Genode::addr_t phys_base, int x, int y, int alpha) {
			_ipu.overlay(phys_base, x, y, alpha); }
};


static bool config_attribute(Genode::Xml_node node, const char *attr_name)
{
	return node.attribute_value(attr_name, false);
}


struct Main
{
	Genode::Env        &_env;
	Genode::Entrypoint &_ep;

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Framebuffer::Driver _driver { _env };

	Framebuffer::Session_component _fb_session { _env, _driver,
		config_attribute(_config.xml(), "buffered") };

	Genode::Static_root<Framebuffer::Session> _fb_root {
		_ep.manage(_fb_session) };

	Main(Genode::Env &env) : _env(env), _ep(_env.ep())
	{
		Genode::log("--- i.MX53 framebuffer driver ---");

		_env.parent().announce(_ep.manage(_fb_root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
