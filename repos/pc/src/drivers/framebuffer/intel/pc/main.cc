/*
 * \brief  Linux Intel framebuffer driver port
 * \author Alexander Boettcher
 * \date   2022-03-08
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <timer_session/connection.h>
#include <capture_session/connection.h>
#include <os/pixel_rgb888.h>
#include <os/reporter.h>
#include <util/reconstructible.h>

/* emulation includes */
#include <lx_emul/init.h>
#include <lx_emul/fb.h>
#include <lx_emul/task.h>
#include <lx_kit/env.h>
#include <lx_kit/init.h>

/* local includes */
extern "C" {
#include "lx_i915.h"
}


extern struct task_struct * lx_user_task;


namespace Framebuffer {
	using namespace Genode;
	struct Driver;
}


struct Framebuffer::Driver
{
	Env                    &env;
	Timer::Connection       timer    { env };
	Attached_rom_dataspace  config   { env, "config" };
	Reporter                reporter { env, "connectors" };

	Signal_handler<Driver>  config_handler { env.ep(), *this,
	                                        &Driver::config_update };
	Signal_handler<Driver>  timer_handler  { env.ep(), *this,
	                                        &Driver::handle_timer };

	class Fb
	{
		private:

			Capture::Connection         _capture;
			Capture::Area const         _size;
			Capture::Area const         _size_phys;
			Capture::Connection::Screen _captured_screen;
			void                      * _base;

			/*
			 * Non_copyable
			 */
			Fb(const Fb&);
			Fb & operator=(const Fb&);

		public:

			void paint()
			{
				using Pixel = Capture::Pixel;
				Surface<Pixel> surface((Pixel*)_base, _size_phys);
				_captured_screen.apply_to_surface(surface);
			}

			Fb(Env & env, void * base, Capture::Area size,
			   Capture::Area size_phys)
			:
				_capture(env),
				_size(size),
				_size_phys(size_phys),
				_captured_screen(_capture, env.rm(), _size),
				_base(base) {}

			bool same_setup(void * base, Capture::Area &size,
			                Capture::Area &size_phys)
			{
				return ((base == _base) && (size == _size) &&
				        (size_phys == _size_phys));
			}
	};

	Constructible<Fb> fb {};

	void config_update();
	void generate_report(void *);
	void lookup_config(char const *, struct genode_mode &mode);

	void handle_timer()
	{
		if (fb.constructed()) { fb->paint(); }
	}

	Driver(Env &env) : env(env)
	{
		Lx_kit::initialize(env);

		config.sigh(config_handler);
	}

	void start()
	{
		log("--- Intel framebuffer driver started ---");

		lx_emul_start_kernel(nullptr);

		timer.sigh(timer_handler);
		timer.trigger_periodic(20*1000);
	}
};


enum { MAX_BRIGHTNESS = 100u };


void Framebuffer::Driver::config_update()
{
	config.update();

	if (!config.valid() || !lx_user_task)
		return;

	lx_emul_task_unblock(lx_user_task);
	Lx_kit::env().scheduler.schedule();
}


static Framebuffer::Driver & driver(Genode::Env & env)
{
	static Framebuffer::Driver driver(env);
	return driver;
}


void Framebuffer::Driver::generate_report(void *lx_data)
{
	/* check for report configuration option */
	try {
		reporter.enabled(config.xml().sub_node("report")
		                 .attribute_value(reporter.name().string(), false));
	} catch (...) {
		Genode::warning("Failed to enable report");
		reporter.enabled(false);
	}

	if (!reporter.enabled()) return;

	try {
		Genode::Reporter::Xml_generator xml(reporter, [&] ()
		{
			lx_emul_i915_report(lx_data, &xml);
		});
	} catch (...) {
		Genode::warning("Failed to generate report");
	}
}


void Framebuffer::Driver::lookup_config(char const * const name,
                                        struct genode_mode &mode)
{
	if (!config.valid())
		return;

	unsigned force_width  = config.xml().attribute_value("force_width",  0u);
	unsigned force_height = config.xml().attribute_value("force_height", 0u);

	/* iterate independently of force* ever to get brightness and hz */
	config.xml().for_each_sub_node("connector", [&] (Xml_node &node) {
		typedef String<32> Name;
		Name const con_policy = node.attribute_value("name", Name());
		if (con_policy != name)
			return;

		mode.enabled = node.attribute_value("enabled", true);
		if (!mode.enabled)
			return;

		mode.brightness = node.attribute_value("brightness",
		                                       unsigned(MAX_BRIGHTNESS + 1));

		mode.width  = node.attribute_value("width",  0U);
		mode.height = node.attribute_value("height", 0U);
		mode.hz     = node.attribute_value("hz", 0U);
		mode.id     = node.attribute_value("mode_id", 0U);
	});

	/* enforce forced width/height if configured */
	mode.preferred = force_width && force_height;
	if (mode.preferred) {
		mode.width  = force_width;
		mode.height = force_height;
		mode.id     = 0;
	}
}


/**
 * Can be called already as side-effect of `lx_emul_start_kernel`,
 * that's why the Driver object needs to be constructed already here.
 */
extern "C" void lx_emul_framebuffer_ready(void * base, unsigned long,
                                          unsigned xres, unsigned yres,
                                          unsigned phys_width,
                                          unsigned phys_height)
{
	auto &env = Lx_kit::env().env;
	auto &drv = driver(env);
	auto &fb  = drv.fb;

	Capture::Area area(xres, yres);
	Capture::Area area_phys(phys_width, phys_height);

	if (fb.constructed()) {
		if (fb->same_setup(base, area, area_phys))
			return;

		fb.destruct();
	}

	/* clear artefacts */
	if (area != area_phys)
		Genode::memset(base, 0, area_phys.count() * 4);

	fb.construct(env, base, area, area_phys);

	Genode::log("framebuffer reconstructed - virtual=", xres, "x", yres,
	            " physical=", phys_width, "x", phys_height);
}


extern "C" void lx_emul_i915_hotplug_connector(void *data)
{
	Genode::Env &env = Lx_kit::env().env;
	driver(env).generate_report(data);
}


void lx_emul_i915_report_connector(void * lx_data, void * genode_xml,
                                   char const *name, char const connected,
                                   unsigned brightness)
{
	auto &xml = *reinterpret_cast<Genode::Reporter::Xml_generator *>(genode_xml);

	xml.node("connector", [&] ()
	{
		xml.attribute("name", name);
		xml.attribute("connected", !!connected);

		/* insane values means no brightness support - we use percentage */
		if (brightness <= MAX_BRIGHTNESS)
			xml.attribute("brightness", brightness);

		lx_emul_i915_iterate_modes(lx_data, &xml);
	});

	/* re-read config on connector change */
	Genode::Signal_transmitter(driver(Lx_kit::env().env).config_handler).submit();
}


void lx_emul_i915_report_modes(void * genode_xml, struct genode_mode *mode)
{
	if (!genode_xml || !mode)
		return;

	auto &xml = *reinterpret_cast<Genode::Reporter::Xml_generator *>(genode_xml);

	xml.node("mode", [&] ()
	{
		xml.attribute("width",     mode->width);
		xml.attribute("height",    mode->height);
		xml.attribute("hz",        mode->hz);
		xml.attribute("mode_id",   mode->id);
		xml.attribute("mode_name", mode->name);
		if (mode->preferred)
			xml.attribute("preferred", true);
	});
}


void lx_emul_i915_connector_config(char * name, struct genode_mode * mode)
{
	if (!mode || !name)
		return;

	Genode::Env &env = Lx_kit::env().env;
	driver(env).lookup_config(name, *mode);
}


void Component::construct(Genode::Env &env)
{
	driver(env).start();
}
