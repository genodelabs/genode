/*
 * \brief  Sculpt framebuffer-driver management
 * \author Norman Feske
 * \date   2024-03-15
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__FB_H_
#define _DRIVER__FB_H_

#include <i2c_session/i2c_session.h>
#include <capture_session/capture_session.h>
#include <gpu_session/gpu_session.h>
#include <platform_session/platform_session.h>
#include <pin_control_session/pin_control_session.h>

namespace Sculpt { struct Fb_driver; }


struct Sculpt::Fb_driver : private Noncopyable
{
	using Fb_name = String<16>;

	struct Action : Interface
	{
		virtual void fb_connectors_changed() = 0;
	};

	Env    &_env;
	Action &_action;

	Constructible<Child_state> _intel_gpu { },
	                           _intel_fb  { },
	                           _vesa_fb   { },
	                           _boot_fb   { },
	                           _soc_fb    { };

	Constructible<Rom_handler<Fb_driver>> _connectors { };

	void _handle_connectors(Node const &) { _action.fb_connectors_changed(); }

	Fb_name _fb_name() const
	{
		return _intel_fb.constructed() ? "intel_fb"
		     : _vesa_fb .constructed() ? "vesa_fb"
		     : _boot_fb .constructed() ? "boot_fb"
		     : _soc_fb  .constructed() ? "fb"
		     : "";
	}

	Fb_driver(Env &env, Action &action) : _env(env), _action(action) { }

	void gen_child_nodes(Generator &g) const
	{
		auto child_node = [&] (auto const &driver, auto const &fn)
		{
			if (driver.constructed())
				g.node("child", [&] {
					driver->gen_child_node_content(g);
					fn(); });
		};

		child_node(_intel_gpu, [&] {
			g.node("provides", [&] {
				g.node("gpu");
				g.node("platform");
			});
			g.tabular_node("connect", [&] {
				connect_platform(g, "intel_gpu");
				connect_config_rom(g, "config", "child/gpu");
				connect_config_rom(g, "system", "system");
				g.node("rm", [&] { g.node("parent"); });
			});
		});

		child_node(_intel_fb, [&] {
			g.node("heartbeat", [&] { });
			g.tabular_node("connect", [&] {
				g.node("platform", [&] { gen_named_node(g, "child", "intel_gpu"); });
				connect_capture(g);
				connect_report(g);
				connect_config_rom(g, "config", "fb");
				connect_report_rom(g, "intel_opregion", "platform/intel_opregion");
				g.node("rm", [&] { g.node("parent"); });
			});
		});

		child_node(_vesa_fb, [&] {
			g.tabular_node("connect", [&] {
				connect_platform(g, "vesa_fb");
				connect_capture(g);
				connect_report(g);
				connect_config_rom(g, "config", "fb");
				g.node("io_mem",  [&] { g.node("parent"); });
				g.node("io_port", [&] { g.node("parent"); });
			});
		});

		child_node(_boot_fb, [&] {
			g.tabular_node("connect", [&] {
				connect_config_rom(g, "config", "fb");
				connect_parent_rom(g, "platform_info");
				g.node("io_mem", [&] { g.node("parent"); });
				connect_capture(g);
				connect_report(g);
			});
		});

		child_node(_soc_fb, [&] {
			g.tabular_node("connect", [&] {
				connect_platform(g, "fb");
				connect_pin_control(g, "PH10", "backlight"); /* pinephone */
				connect_pin_control(g, "PD23", "lcd-reset"); /* pinephone */
				connect_i2c(g);
				connect_capture(g);
				connect_report(g);
				connect_config_rom(g, "config", "fb");
				connect_parent_rom(g, "dtb",    "fb.dtb");
				g.node("rm", [&] { g.node("parent"); });
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info,
	            Node const &platform)
	{
		bool const suspending  = board_info.options.suspending;

		bool const use_intel_gpu =  board_info.detected.intel_gfx &&
		                           !board_info.options.suppress.intel_gpu;
		bool const use_intel_fb  =  use_intel_gpu && !suspending;
		bool const use_boot_fb   = !use_intel_fb  && !suspending &&
		                            board_info.detected.boot_fb;
		bool const use_vesa      = !use_intel_fb  && !suspending &&
		                            board_info.detected.vga && !use_boot_fb;

		Fb_name const orig_fb_name = _fb_name();

		_intel_gpu.conditional(use_intel_gpu, registry, Priority::MULTIMEDIA,
		                       Child_name { "intel_gpu" }, Binary_name { "intel_gpu" },
		                       Ram_quota { 32*1024*1024 }, Cap_quota { 1400 });

		_intel_fb.conditional(use_intel_fb, registry, Priority::MULTIMEDIA,
		                      Child_name { "intel_fb" }, Binary_name { "pc_intel_fb" },
		                      Ram_quota { 16*1024*1024 }, Cap_quota { 800 });

		_vesa_fb.conditional(use_vesa, registry, Priority::MULTIMEDIA,
		                     Child_name { "vesa_fb" }, Binary_name { "vesa_fb" },
		                     Ram_quota { 8*1024*1024 }, Cap_quota { 110 });

		Affinity::Location const fb_affinity =
			board_info.soc.fb_on_dedicated_cpu ? Affinity::Location { 1, 0, 1, 1 }
			                                   : Affinity::Location { };

		_soc_fb.conditional(board_info.soc.fb && board_info.options.display,
		                    registry, Child_state::Attr {
		                        .name      = "fb",
		                        .binary    = "fb",
		                        .priority  = Priority::MULTIMEDIA,
		                        .location  = fb_affinity,
		                        .initial   = { Ram_quota { 16*1024*1024 },
		                                       Cap_quota { 250 } },
		                        .max       = { } } );

		if (use_boot_fb && !_boot_fb.constructed())
			Boot_fb::with_mode(platform, [&] (Boot_fb::Mode mode) {
				_boot_fb.construct(registry, Priority::MULTIMEDIA,
				                   Child_name { "boot_fb" }, Binary_name { "boot_fb" },
				                   mode.ram_quota(), Cap_quota { 100 }); });

		if (orig_fb_name != _fb_name()) {
			Session_label label { "report -> ", _fb_name(), "/connectors" };
			if (_fb_name().length() > 1)
				_connectors.construct(_env, label,
				                      *this, &Fb_driver::_handle_connectors);
			else
				_connectors.destruct();
		}
	}

	static bool suspend_supported(Board_info const &board_info)
	{
		/* offer suspend/resume only when using intel graphics */
		return board_info.detected.intel_gfx
		   && !board_info.options.suppress.intel_gpu;
	}

	void with_connectors(auto const &fn) const
	{
		if (_connectors.constructed())
			_connectors->with_node(fn);
	}
};

#endif /* _DRIVER__FB_H_ */
