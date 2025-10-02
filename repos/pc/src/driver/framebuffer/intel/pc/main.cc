/*
 * \brief  Linux Intel framebuffer driver port
 * \author Alexander Boettcher
 * \date   2022-03-08
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <capture_session/connection.h>
#include <os/pixel_rgb888.h>
#include <os/reporter.h>
#include <util/reconstructible.h>

/* emulation includes */
#include <lx_emul/init.h>
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
	Heap                    heap     { env.ram(), env.rm() };
	Attached_rom_dataspace  config   { env, "config" };
	Expanding_reporter      reporter { env, "connectors", "connectors" };

	Signal_handler<Driver>  process_handler   { env.ep(), *this,
	                                            &Driver::process_action };
	Signal_handler<Driver>  config_handler    { env.ep(), *this,
	                                            &Driver::config_update };
	Signal_handler<Driver>  scheduler_handler { env.ep(), *this,
	                                            &Driver::handle_scheduler };

	bool                    merge_label_changed { false };
	bool                    verbose             { false };

	Capture::Connection::Label merge_label { "mirror" };

	char const * action_name(enum Action const action)
	{
		switch (action) {
		case ACTION_IDLE         : return "IDLE";
		case ACTION_DETECT_MODES : return "DETECT_MODES";
		case ACTION_CONFIGURE    : return "CONFIGURE";
		case ACTION_REPORT       : return "REPORT";
		case ACTION_NEW_CONFIG   : return "NEW_CONFIG";
		case ACTION_READ_CONFIG  : return "READ_CONFIG";
		case ACTION_HOTPLUG      : return "HOTPLUG";
		case ACTION_FAILED       : return "FAILED";
		}
		return "UNKNOWN";
	}

	enum Action active      { };
	enum Action pending[31] { };

	void add_action(enum Action const add, bool may_squash = false)
	{
		Action * prev { };

		for (auto &entry : pending) {
			if (entry != ACTION_IDLE) {
				prev = &entry;
				continue;
			}

			/* skip in case the last entry contains the very same to be added */
			if (may_squash && prev && add == *prev) {
				if (verbose)
					error("action already queued - '", action_name(add), "'");
				return;
			}

			entry = add;

			if (verbose)
				error("action added to queue - '", action_name(add), "'");

			return;
		}

		error("action ", action_name(add), " NOT QUEUED - trouble ahead");
	}

	auto next_action()
	{
		Action   next { ACTION_IDLE };
		Action * prev { };

		for (auto &entry : pending) {
			if (next == ACTION_IDLE)
				next = entry;

			if (prev)
				*prev = entry;

			prev = &entry;
		}

		if (prev)
			*prev = ACTION_IDLE;

		if (verbose)
			error("action now executing  - '", action_name(next), "'");

		active = next;

		return active;
	}

	bool action_in_execution() const { return active != ACTION_IDLE; }

	struct Connector {
		using Space = Id_space<Connector>;
		using Id    = Space::Id;

		Space::Element            id_element;
		Signal_handler<Connector> capture_wakeup;

		addr_t        base      { };
		Capture::Area size      { };
		Capture::Area size_phys { };
		Capture::Area size_mm   { };
		Blit::Rotate  rotate    { };
		Blit::Flip    flip      { };

		Constructible<Capture::Connection>         capture { };
		Constructible<Capture::Connection::Screen> screen  { };

		Connector(Env &env, Space &space, Id id)
		:
			id_element(*this, space, id),
			capture_wakeup(env.ep(), *this, &Connector::wakeup_handler)
		{ }

		void wakeup_handler()
		{
			lx_emul_i915_wakeup(unsigned(id_element.id().value));
			Lx_kit::env().scheduler.execute();
		}
	};

	Connector::Space ids { };

	bool capture(Connector::Space &ids, Connector::Id const &id, bool const may_stop)
	{
		using Pixel = Capture::Pixel;

		bool dirty = false;

		ids.apply<Connector>(id, [&](Connector &connector) {

			if (!connector.capture.constructed() ||
			    !connector.screen.constructed())
				return;

			Surface<Pixel> surface((Pixel*)connector.base, connector.size_phys);

			auto box = connector.screen->apply_to_surface(surface);

			if (box.valid())
				dirty = true;

			if (!dirty && may_stop)
				connector.capture->capture_stopped();

		}, [&] () { /* unknown connector id -> no dirty content */ });

		return dirty;
	}

	bool update(Connector           &conn,
	            addr_t        const  base,
	            Capture::Area const &size,
	            Capture::Area const &size_phys,
	            Capture::Area const &mm,
	            auto          const &label,
	            auto          const  rotate,
	            auto          const  flip,
	            bool          const  force_change)
	{
		bool same = (base         == conn.base) &&
		            (size         == conn.size) &&
		            (size_phys    == conn.size_phys) &&
		            (mm           == conn.size_mm) &&
		            (rotate       == conn.rotate) &&
		            (flip.enabled == conn.flip.enabled) &&
		            !force_change;

		if (same)
			return same;

		conn.base      = base;
		conn.size      = size;
		conn.size_phys = size_phys;
		conn.size_mm   = mm;
		conn.rotate    = rotate;
		conn.flip      = flip;

		conn.screen .destruct();
		conn.capture.destruct();

		if (!conn.size.valid())
			return same;

		auto const r_ps = Blit::transformed(conn.size_phys, conn.rotate);
		auto const r_vs = Blit::transformed(conn.size     , conn.rotate);

		Point const p_90  = { .x = 0, .y = int(r_ps.h - r_vs.h) };
		Point const p_180 = { .x = int(r_ps.w - r_vs.w),
		                      .y = int(r_ps.h - r_vs.h) };
		Point const p_270 = { .x = int(r_ps.w - r_vs.w), .y = 0 };

		Point po = { };

		switch (conn.rotate) {
			case Blit::Rotate::R0  : po = !flip.enabled ? po    : p_270; break;
			case Blit::Rotate::R90 : po = !flip.enabled ? p_90  : po;    break;
			case Blit::Rotate::R180: po = !flip.enabled ? p_180 : p_90;  break;
			case Blit::Rotate::R270: po = !flip.enabled ? p_270 : p_180; break;
		}

		using Attr = Capture::Connection::Screen::Attr;

		Attr attr = { .px       = r_ps,
		              .mm       = Blit::transformed(conn.size_mm, conn.rotate),
		              .viewport = { .at = po, .area = r_vs },
		              .rotate   = conn.rotate,
		              .flip     = conn.flip };

		conn.capture.construct(env, label);
		conn.screen .construct(*conn.capture, env.rm(), attr);

		conn.capture->wakeup_sigh(conn.capture_wakeup);

		return same;
	}

	void process_action();
	void config_update();
	void config_read();
	void generate_report();
	void lookup_config(char const *, struct genode_mode &mode);

	void handle_scheduler()
	{
		Lx_kit::env().scheduler.execute();
	}

	Driver(Env &env) : env(env)
	{
		Lx_kit::initialize(env, scheduler_handler);

		/*
		 * Delay startup of driver until graphic device is available.
		 * After resume it is possible, that no device is instantly available.
		 * This ported Linux driver hangs up otherwise, when the delayed
		 * Device announcement is handled later inside the lx_kit for unknown
		 * reasons.
		 */
		Lx_kit::env().devices.for_each([](auto &device) {
			/*
			 * Only iterate over intel devices, other rendering devices might
			 * be visibale depending on the policy filtering rule of
			 * the platform driver.
			 */
			device.for_pci_config([&] (auto &cfg) {
				if (cfg.vendor_id == 0x8086) {
					/* only enable graphic device and skip bridge, which has no irq atm */
					device.for_each_irq([&](auto &) { device.enable(); });
				}
			});
		});

		config.sigh(config_handler);

		config_read();
	}

	void start()
	{
		log("--- Intel framebuffer driver started ---");

		lx_emul_start_kernel(nullptr);
	}

	bool apply_config_on_hotplug() const
	{
		bool apply_config = true;

		if (config.valid())
			apply_config = config.node().attribute_value("apply_on_hotplug", apply_config);

		return apply_config;
	}

	void with_max_enforcement(auto const &fn) const
	{
		unsigned max_width  = config.node().attribute_value("max_width", 0u);
		unsigned max_height = config.node().attribute_value("max_height",0u);

		if (max_width && max_height)
			fn(max_width, max_height);
	}

	void with_force(auto const &node, auto const &fn) const
	{
		unsigned force_width  = node.attribute_value("width",  0u);
		unsigned force_height = node.attribute_value("height", 0u);

		if (force_width && force_height)
			fn(force_width, force_height);
	}

	unsigned long long max_framebuffer_memory()
	{
		/*
		 * The max framebuffer memory is virtual in nature and denotes how
		 * the driver sizes its buffer space. When actual memory is used and
		 * the available RAM quota is not enough the component will issue a
		 * resource request.
		 *
		 * As the available memory is used during the initialization of the
		 * driver and is not queried afterwards it is safe to acquired it
		 * only once. Since it is used to size the GEM buffer pool set the amount
		 * of memory so that it includes the currently anticipated resolutions
		 * (e.g. 3840x2160) and is in line with the default value of the Intel GPU
		 * multiplexer.
		 */
		static unsigned long long _framebuffer_memory = 0;

		if (_framebuffer_memory)
			return _framebuffer_memory;

		enum : unsigned { DEFAULT_FB_MEMORY = 64u << 20, };
		auto framebuffer_memory = Number_of_bytes(DEFAULT_FB_MEMORY);
		if (config.valid())
			framebuffer_memory =
				config.node().attribute_value("max_framebuffer_memory",
				                              framebuffer_memory);

		if (framebuffer_memory < DEFAULT_FB_MEMORY) {
			warning("configured framebuffer memory too small, use default of ",
			        Number_of_bytes(DEFAULT_FB_MEMORY));
			framebuffer_memory = Number_of_bytes(DEFAULT_FB_MEMORY);
		}
		_framebuffer_memory = framebuffer_memory;

		return _framebuffer_memory;
	}
};


enum { MAX_BRIGHTNESS = 100u };


void Framebuffer::Driver::process_action()
{
	if (action_in_execution())
		return;

	if (!lx_user_task)
		return;

	lx_emul_task_unblock(lx_user_task);
	Lx_kit::env().scheduler.execute();
}


void Framebuffer::Driver::config_update()
{
	add_action(Action::ACTION_NEW_CONFIG, true);

	if (action_in_execution())
		return;

	Genode::Signal_transmitter(process_handler).submit();
}


void Framebuffer::Driver::config_read()
{
	config.update();

	if (!config.valid())
		return;

	config.node().with_optional_sub_node("merge", [&](auto const &node) {
		auto const merge_label_before = merge_label;

		merge_label = node.attribute_value("name", String<160>("mirror"));

		merge_label_changed = merge_label_before != merge_label;
	});
}


static Framebuffer::Driver & driver(Genode::Env &env)
{
	static Framebuffer::Driver driver(env);
	return driver;
}


void Framebuffer::Driver::generate_report()
{
	if (!config.valid()) {
		error("no valid config - report is dropped");
		return;
	}

	/* check for report configuration option */
	config.node().with_optional_sub_node("report", [&](auto const &node) {

		if (!node.attribute_value("connectors", false))
			return;

		reporter.generate([&] (Genode::Generator &g) {
			/* reflect force/max enforcement in report for user clarity */
			with_max_enforcement([&](unsigned width, unsigned height) {
				g.attribute("max_width",  width);
				g.attribute("max_height", height);
			});

			lx_emul_i915_report_discrete(&g);

			g.node("merge", [&] () {
				g.attribute("name", merge_label);
				node.with_optional_sub_node("merge", [&](auto const &merge) {
					with_force(merge, [&](unsigned width, unsigned height) {
						g.attribute("width",  width);
						g.attribute("height", height);
					});
				});

				lx_emul_i915_report_non_discrete(&g);
			});
		});
	});
}


void Framebuffer::Driver::lookup_config(char const * const name,
                                        struct genode_mode &mode)
{
	bool mirror_node = false;

	/* default settings, possibly overridden by explicit configuration below */
	mode.enabled    = true;
	mode.brightness = 70; /* percent */
	mode.mirror     = true;

	if (!config.valid())
		return;

	with_max_enforcement([&](unsigned const width, unsigned const height) {
		mode.max_width  = width;
		mode.max_height = height;
	});

	auto for_each_node = [&](auto const &node, bool const mirror){
		using Name = String<32>;
		Name const con_policy = node.attribute_value("name", Name());
		if (con_policy != name)
			return;

		mode.mirror  = mirror;
		mode.enabled = node.attribute_value("enabled", true);

		if (!mode.enabled)
			return;

		mode.width      = node.attribute_value("width"  , 0U);
		mode.height     = node.attribute_value("height" , 0U);
		mode.hz         = node.attribute_value("hz"     , 0U);
		mode.id         = node.attribute_value("mode"   , 0U);
		mode.rotate     = node.attribute_value("rotate" , 0U);
		mode.flip       = node.attribute_value("flip"   , false);
		mode.brightness = node.attribute_value("brightness",
		                                       unsigned(MAX_BRIGHTNESS + 1));
	};

	/* lookup config of discrete connectors */
	config.node().for_each_sub_node("connector", [&] (Node const &conn) {
		for_each_node(conn, false);
	});

	/* lookup config of mirrored connectors */
	config.node().for_each_sub_node("merge", [&] (Node const &merge) {
		if (mirror_node) {
			error("only one mirror node supported");
			return;
		}

		merge.for_each_sub_node("connector", [&] (Node const &conn) {
			for_each_node(conn, true);
		});

		with_force(merge, [&](unsigned const width, unsigned const height) {
			mode.force_width  = width;
			mode.force_height = height;
		});

		mirror_node = true;
	});
}


unsigned long long driver_max_framebuffer_memory(void)
{
	Genode::Env &env = Lx_kit::env().env;
	return driver(env).max_framebuffer_memory();
}


void lx_emul_i915_framebuffer_ready(unsigned const connector_id,
                                    char const * const conn_name,
                                    void * const base,
                                    unsigned long,
                                    unsigned const xres,
                                    unsigned const yres,
                                    unsigned const phys_width,
                                    unsigned const phys_height,
                                    unsigned const mm_width,
                                    unsigned const mm_height,
                                    unsigned const lx_rotate,
                                    char     const lx_flip)
{
	auto &env = Lx_kit::env().env;
	auto &drv = driver(env);

	using namespace Genode;

	typedef Framebuffer::Driver::Connector Connector;

	auto const id = Connector::Id { connector_id };

	/* allocate new id for new connector */
	drv.ids.apply<Connector>(id, [&](Connector &) { /* known id */ }, [&](){
		/* ignore unused connector - don't need a object for it */
		if (!base)
			return;

		new (drv.heap) Connector (env, drv.ids, id);
	});

	drv.ids.apply<Connector>(id, [&](Connector &conn) {

		Capture::Area const area     (xres, yres);
		Capture::Area const area_phys(phys_width, phys_height);

		Blit::Rotate  rotate    { (lx_rotate ==  90) ? Blit::Rotate::R90  :
		                          (lx_rotate == 180) ? Blit::Rotate::R180 :
		                          (lx_rotate == 270) ? Blit::Rotate::R270 :
		                                               Blit::Rotate::R0 };
		Blit::Flip    flip      { .enabled = !!lx_flip };

		bool const merge = Capture::Connection::Label(conn_name) == "mirror_capture";

		auto const label = !conn_name
		                 ? Capture::Connection::Label(conn.id_element)
		                 : merge ? drv.merge_label
		                         : Capture::Connection::Label(conn_name);

		bool const same = drv.update(conn, Genode::addr_t(base), area,
		                             area_phys, { mm_width, mm_height}, label,
		                             rotate, flip,
		                             merge && drv.merge_label_changed);

		if (merge)
			drv.merge_label_changed = false;

		if (same) {
			lx_emul_i915_wakeup(unsigned(id.value));
			return;
		}

		/* clear artefacts */
		if (base && (area != area_phys))
			Genode::memset(base, 0, area_phys.count() * 4);

		String<12> space { };
		for (auto i = label.length(); i < space.capacity() - 1; i++) {
			space = String<12>(" ", space);
		}

		if (conn.size.valid()) {
			if (drv.verbose)
				log(space, label, ": capture ", xres, "x", yres, " with "
				    " framebuffer ", phys_width, "x", phys_height);

			lx_emul_i915_wakeup(unsigned(id.value));
		} else
			if (drv.verbose)
				log(space, label, ": capture closed ",
				    merge ? "(was mirror capture)" : "");

	}, [](){ /* unknown id */ });
}


void lx_emul_i915_hotplug_connector()
{
	auto &drv = driver(Lx_kit::env().env);

	drv.add_action(Action::ACTION_HOTPLUG, true);

	Genode::Signal_transmitter(drv.process_handler).submit();
}


int lx_emul_i915_action_to_process(int const action_failed)
{
	auto &env = Lx_kit::env().env;

	while (true) {

		auto const action = driver(env).next_action();

		switch (action) {
		case Action::ACTION_HOTPLUG:
			if (driver(env).apply_config_on_hotplug()) {
				driver(env).add_action(Action::ACTION_DETECT_MODES);
				driver(env).add_action(Action::ACTION_CONFIGURE);
				driver(env).add_action(Action::ACTION_REPORT);
			} else {
				driver(env).add_action(Action::ACTION_DETECT_MODES);
				driver(env).add_action(Action::ACTION_REPORT);
			}
			break;
		case Action::ACTION_NEW_CONFIG:
			driver(env).add_action(Action::ACTION_READ_CONFIG);
			driver(env).add_action(Action::ACTION_CONFIGURE);
			driver(env).add_action(Action::ACTION_REPORT);
			break;
		case Action::ACTION_READ_CONFIG:
			driver(env).config_read();
			break;
		case Action::ACTION_REPORT:
			if (action_failed) {
				if (driver(env).verbose)
					Genode::warning("previous action failed");

				/* retry */
				driver(env).add_action(Action::ACTION_HOTPLUG, true);
			} else
				driver(env).generate_report();
			break;
		default:
			/* other actions are handled by Linux code */
			return action;
		}
	}
}


void lx_emul_i915_report_connector(void * lx_data, void * genode_generator,
                                   char const *name, char const connected,
                                   char const /* fb_available */,
                                   unsigned brightness,
                                   char const *display_name,
                                   unsigned width_mm, unsigned height_mm,
                                   unsigned rotate, char flip)
{
	auto &g = *reinterpret_cast<Genode::Generator *>(genode_generator);

	g.node("connector", [&] ()
	{
		g.attribute("connected", !!connected);
		g.attribute("name", name);
		if (display_name)
			g.attribute("display_name", display_name);
		if (width_mm)
			g.attribute("width_mm" , width_mm);
		if (height_mm)
			g.attribute("height_mm", height_mm);
		g.attribute("rotate", rotate);
		g.attribute("flip", !!flip);

		/* insane values means no brightness support - we use percentage */
		if (brightness <= MAX_BRIGHTNESS)
			g.attribute("brightness", brightness);

		lx_emul_i915_iterate_modes(lx_data, &g);
	});
}


void lx_emul_i915_report_modes(void * genode_generator, struct genode_mode *mode)
{
	if (!genode_generator || !mode)
		return;

	auto &g = *reinterpret_cast<Genode::Generator *>(genode_generator);

	g.node("mode", [&] ()
	{
		g.attribute("width",  mode->width);
		g.attribute("height", mode->height);
		g.attribute("hz",     mode->hz);
		g.attribute("id",     mode->id);
		g.attribute("name",   mode->name);
		if (mode->width_mm)
			g.attribute("width_mm",  mode->width_mm);
		if (mode->height_mm)
			g.attribute("height_mm", mode->height_mm);
		if (!mode->enabled)
			g.attribute("usable", false);
		if (mode->preferred)
			g.attribute("preferred", true);
		if (mode->inuse)
			g.attribute("used", true);
	});
}


int lx_emul_i915_blit(unsigned const connector_id, char const may_stop)
{
	auto &drv = driver(Lx_kit::env().env);

	auto const id = Framebuffer::Driver::Connector::Id { connector_id };

	return drv.capture(drv.ids, id, may_stop);
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
