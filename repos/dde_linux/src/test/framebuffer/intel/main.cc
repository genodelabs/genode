/*
 * \brief  Test controller for Intel framebuffer driver
 * \author Stefan Kalkowski
 * \date   2016-04-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <base/attached_rom_dataspace.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>

using namespace Genode;


struct Framebuffer_controller
{
	Attached_rom_dataspace                 rom;
	Signal_handler<Framebuffer_controller> rom_sigh;
	Heap                                   heap;
	Allocator_avl                          fs_alloc;
	File_system::Connection                fs;

	void update_connector_config(Xml_generator & xml, Xml_node & node);
	void update_fb_config(Xml_node & report);
	void report_changed();

	Framebuffer_controller(Env &env)
	: rom("connectors"),
	  rom_sigh(env.ep(), *this, &Framebuffer_controller::report_changed),
	  heap(env.ram(), env.rm()),
	  fs_alloc(&heap),
	  fs(fs_alloc, 128*1024, "")
	{
		rom.sigh(rom_sigh);
	}
};


void Framebuffer_controller::update_connector_config(Xml_generator & xml,
                                                     Xml_node & node)
{
	xml.node("connector", [&] {
		String<64> name;
		node.attribute("name").value(&name);
		xml.attribute("name", name.string());

		bool connected = node.attribute_value("connected", false);
		xml.attribute("enabled", connected ? "true" : "false");

		unsigned long width = 0, height = 0;
		node.for_each_sub_node("mode", [&] (Xml_node &mode) {
			unsigned long w, h;
			w = mode.attribute_value<unsigned long>("width", 0);
			h = mode.attribute_value<unsigned long>("height", 0);
			if (w > width) {
				width = w;
				height = h;
			}
		});

		if (width && height) {
			xml.attribute("width", width);
			xml.attribute("height", height);
		}
	});
}


void Framebuffer_controller::update_fb_config(Xml_node & report)
{
	try {
		static char buf[4096];

		Xml_generator xml(buf, sizeof(buf), "config", [&] {
			xml.node("report", [&] {
				xml.attribute("connectors", "yes");
			});

			report.for_each_sub_node("connector", [&] (Xml_node &node) {
			                         update_connector_config(xml, node); });
		});
		buf[xml.used()] = 0;

		File_system::Dir_handle root_dir = fs.dir("/", false);
		File_system::File_handle file =
			fs.file(root_dir, "fb_drv.config", File_system::READ_WRITE, false);
		if (File_system::write(fs, file, buf, xml.used()) == 0)
			error("Could not write config");
		fs.close(file);
	} catch (...) {
		error("Cannot update config");
	}
}


void Framebuffer_controller::report_changed()
{
	rom.update();
	if (!rom.is_valid()) return;

	Xml_node report(rom.local_addr<char>(), rom.size());
	update_fb_config(report);
}


void Component::construct(Genode::Env &env)
{
	log("--- Framebuffer controller ---\n");
	static Framebuffer_controller controller(env);
}
