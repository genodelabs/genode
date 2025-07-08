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
		_config.node().attribute_value("artifical_update_ms", (uint64_t)0);

	Root_directory _root_dir = _config.node().with_sub_node("vfs",
		[&] (Node const &config) -> Root_directory {
			return { _env, _heap, config }; },
		[&] () -> Root_directory {
			error("VFS not configured");
			return { _env, _heap, Node() }; });

	Timer::Connection                      _timer { _env };
	Signal_handler<Framebuffer_controller> _timer_handler {
		_env.ep(), *this, &Framebuffer_controller::_handle_timer };

	void _update_connector_config(Generator &, Node const &);
	void _update_fb_config(Node const &report);
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


void Framebuffer_controller::_update_connector_config(Generator &g,
                                                      Node const &node)
{
	g.node("connector", [&] {

		g.attribute("name", node.attribute_value("name", String<64>()));

		bool connected = node.attribute_value("connected", false);
		g.attribute("enabled", connected ? "true" : "false");

		unsigned long width = 0, height = 0, hz = 0;
		node.for_each_sub_node("mode", [&] (Node const &mode) {
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
			g.attribute("width", width);
			g.attribute("height", height);
			g.attribute("hz", hz);
			g.attribute("brightness", 73);
		}
	});
}


void Framebuffer_controller::_update_fb_config(Node const &report)
{
	static char buf[4096];

	Generator::generate({ buf, sizeof(buf) - 1 }, "config", [&] (Generator &g) {
		g.attribute("apply_on_hotplug", "no");
		g.node("report", [&] {
			g.attribute("connectors", "yes");
		});

		report.for_each_sub_node("connector", [&] (Node const &node) {
		                         _update_connector_config(g, node); });
	}).with_result(
		[&] (size_t used) {
			buf[used] = 0;
			try {
				New_file file { _root_dir, "fb.config" };
				file.append(buf, used + 1);
			} catch (...) { error("failed to write config to file"); }
		},
		[&] (Buffer_error) { error("config exceeds maximum buffer size"); }
	);
}


void Framebuffer_controller::_handle_connectors()
{
	_connectors.update();

	_update_fb_config(_connectors.node());
}


void Framebuffer_controller::_handle_timer()
{
	/* artificial update */
	_update_fb_config(_connectors.node());
}


void Component::construct(Genode::Env &env)
{
	log("--- Framebuffer controller ---\n");
	static Framebuffer_controller controller(env);
}
