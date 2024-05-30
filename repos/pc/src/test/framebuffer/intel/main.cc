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
#include <os/vfs.h>
#include <base/attached_rom_dataspace.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>
#include <timer_session/connection.h>

using namespace Genode;


struct Framebuffer_controller
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _connectors { _env, "connectors" };

	Signal_handler<Framebuffer_controller> _connectors_handler {
		_env.ep(), *this, &Framebuffer_controller::_handle_connectors };

	Attached_rom_dataspace _config { _env, "config" };

	uint64_t const _period_ms =
		_config.xml().attribute_value("artifical_update_ms", (uint64_t)0);

	Root_directory _root_dir { _env, _heap, _config.xml().sub_node("vfs") };

	Timer::Connection                      _timer { _env };
	Signal_handler<Framebuffer_controller> _timer_handler {
		_env.ep(), *this, &Framebuffer_controller::_handle_timer };

	void _update_connector_config(Xml_generator & xml, Xml_node & node);
	void _update_fb_config(Xml_node const &report);
	void _handle_connectors();
	void _handle_timer();

	Framebuffer_controller(Env &env) : _env(env)
	{
		_connectors.sigh(_connectors_handler);
		_handle_connectors();

		if (_period_ms) {
			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(_period_ms * 1000 /* in us */);
		}
	}
};


void Framebuffer_controller::_update_connector_config(Xml_generator & xml,
                                                      Xml_node & node)
{
	xml.node("connector", [&] {

		xml.attribute("name", node.attribute_value("name", String<64>()));

		bool connected = node.attribute_value("connected", false);
		xml.attribute("enabled", connected ? "true" : "false");

		unsigned long width = 0, height = 0, hz = 0;
		node.for_each_sub_node("mode", [&] (Xml_node &mode) {
			unsigned long w, h, z;
			w = mode.attribute_value<unsigned long>("width", 0);
			h = mode.attribute_value<unsigned long>("height", 0);
			z = mode.attribute_value<unsigned long>("hz", 0);
			if (w * h >= width * height) {
				width = w;
				height = h;
				if (z > hz)
					hz = z;
			}
		});

		if (width && height) {
			xml.attribute("width", width);
			xml.attribute("height", height);
			xml.attribute("hz", hz);
			xml.attribute("brightness", 73);
		}
	});
}


void Framebuffer_controller::_update_fb_config(Xml_node const &report)
{
	try {
		static char buf[4096];

		Xml_generator xml(buf, sizeof(buf), "config", [&] {
			xml.attribute("apply_on_hotplug", "no");
			xml.node("report", [&] {
				xml.attribute("connectors", "yes");
			});

			report.for_each_sub_node("connector", [&] (Xml_node &node) {
			                         _update_connector_config(xml, node); });
		});
		buf[xml.used()] = 0;

		{
			New_file file { _root_dir, "fb.config" };

			file.append(buf, xml.used());
		}

	} catch (...) {
		error("Cannot update config");
	}
}


void Framebuffer_controller::_handle_connectors()
{
	_connectors.update();

	_update_fb_config(_connectors.xml());
}


void Framebuffer_controller::_handle_timer()
{
	/* artificial update */
	_update_fb_config(_connectors.xml());
}


void Component::construct(Genode::Env &env)
{
	log("--- Framebuffer controller ---\n");
	static Framebuffer_controller controller(env);
}
