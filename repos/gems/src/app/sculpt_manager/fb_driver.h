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

#ifndef _FB_DRIVER_H_
#define _FB_DRIVER_H_

/* local includes */
#include <model/child_exit_state.h>
#include <model/board_info.h>
#include <runtime.h>

namespace Sculpt { struct Fb_driver; }


struct Sculpt::Fb_driver : private Noncopyable
{
	Constructible<Child_state> _intel_gpu { },
	                           _intel_fb  { },
	                           _vesa_fb   { },
	                           _boot_fb   { },
	                           _soc_fb    { };

	void gen_start_nodes(Xml_generator &xml) const
	{
		auto gen_common_routes = [&] (Xml_generator &xml)
		{
			gen_parent_route<Rom_session>     (xml);
			gen_parent_route<Cpu_session>     (xml);
			gen_parent_route<Pd_session>      (xml);
			gen_parent_route<Log_session>     (xml);
			gen_parent_route<Timer::Session>  (xml);
			gen_parent_route<Report::Session> (xml);
		};

		auto gen_capture_route = [&] (Xml_generator &xml)
		{
			gen_service_node<Capture::Session>(xml, [&] {
				xml.node("parent", [&] {
					xml.attribute("label", "global"); }); });
		};

		auto start_node = [&] (auto const &driver, auto const &binary, auto const &fn)
		{
			if (driver.constructed())
				xml.node("start", [&] {
					driver->gen_start_node_content(xml);
					gen_named_node(xml, "binary", binary);
					fn(); });
		};

		start_node(_intel_gpu, "intel_gpu_drv", [&] {
			xml.node("provides", [&] {
				gen_service_node<Gpu::Session>     (xml, [&] () { });
				gen_service_node<Platform::Session>(xml, [&] () { });
			});
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>(xml);
				gen_parent_rom_route(xml, "config", "config -> gpu_drv");
				gen_parent_route<Rm_session>(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_intel_fb, "pc_intel_fb_drv", [&] {
			xml.node("route", [&] {
				gen_service_node<Platform::Session>(xml, [&] {
					gen_named_node(xml, "child", "intel_gpu"); });
				gen_capture_route(xml);
				gen_parent_rom_route(xml, "config", "config -> fb_drv");
				gen_parent_rom_route(xml, "intel_opregion", "report -> drivers/intel_opregion");
				gen_parent_route<Rm_session>(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_vesa_fb, "vesa_fb_drv", [&] {
			xml.node("route", [&] () {
				gen_parent_route<Platform::Session>(xml);
				gen_capture_route(xml);
				gen_parent_rom_route(xml, "config", "config -> fb_drv");
				gen_parent_route<Io_mem_session>(xml);
				gen_parent_route<Io_port_session>(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_boot_fb, "boot_fb_drv", [&] {
			xml.node("route", [&] () {
				gen_parent_rom_route(xml, "config", "config -> fb_drv");
				gen_parent_route<Io_mem_session>(xml);
				gen_capture_route(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_soc_fb, "fb_drv", [&] {
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>   (xml);
				gen_parent_route<Pin_control::Session>(xml);
				gen_capture_route(xml);
				gen_parent_rom_route(xml, "config", "config -> fb_drv");
				gen_parent_rom_route(xml, "dtb",    "fb_drv.dtb");
				gen_parent_route<Rm_session>(xml);
				gen_common_routes(xml);
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info,
	            Xml_node const &platform)
	{
		_intel_gpu.conditional(board_info.intel_gfx_present,
		                       registry, "intel_gpu", Priority::MULTIMEDIA,
		                       Ram_quota { 32*1024*1024 }, Cap_quota { 1400 });

		_intel_fb.conditional(board_info.intel_gfx_present,
		                      registry, "intel_fb", Priority::MULTIMEDIA,
		                      Ram_quota { 16*1024*1024 }, Cap_quota { 800 });

		_vesa_fb.conditional(board_info.vesa_fb_present,
		                     registry, "vesa_fb", Priority::MULTIMEDIA,
		                     Ram_quota { 8*1024*1024 }, Cap_quota { 110 });

		_soc_fb.conditional(board_info.soc_fb_present,
		                    registry, "fb", Priority::MULTIMEDIA,
		                    Ram_quota { 16*1024*1024 }, Cap_quota { 250 });

		if (board_info.boot_fb_present && !_boot_fb.constructed())
			Boot_fb::with_mode(platform, [&] (Boot_fb::Mode mode) {
				_boot_fb.construct(registry, "boot_fb", Priority::MULTIMEDIA,
				                   mode.ram_quota(), Cap_quota { 100 }); });
	}
};

#endif /* _FB_DRIVER_H_ */
