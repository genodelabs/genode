/*
 * \brief  PC driver discovery for Sculpt OS
 * \author Norman Feske
 * \date   2026-04-13
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/hid_edit.h>
#include <os/reporter.h>

/* local includes */
#include <board_info.h>
#include <boot_fb.h>

namespace Pc_discover { struct Main; }


struct Pc_discover::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _orig_board    { _env, "board" },
	                       _devices       { _env, "devices" },
	                       _platform_info { _env, "platform_info" };

	Signal_handler<Main> _devices_handler { _env.ep(), *this, &Main::_handle_devices };

	void _handle_devices();

	Main(Env &env) : _env(env)
	{
		_devices.sigh(_devices_handler);
		_handle_devices();
	}
};


void Pc_discover::Main::_handle_devices()
{
	using Value = String<32>;

	_devices.update();

	Node const &orig     = _orig_board.node(),
	           &devices  = _devices.node(),
	           &platform = _platform_info.node();

	if (devices.type() == "empty")
		return;

	struct Have_driver { bool intel_gpu, intel_fb; } have_driver { };

	orig.for_each_sub_node("child", [&] (Node const &node) {
		Value const name = node.attribute_value("name", Value());
		if (name == "intel_gpu") have_driver.intel_gpu = true;
		if (name == "intel_fb")  have_driver.intel_fb  = true;
	});

	auto const detected = Board_info::Detected::from_node(devices, platform);

	bool const intel_fb_usable = have_driver.intel_fb && detected.intel_gfx;

	log("detected ", detected);

	Hid_edit edit { _heap, _orig_board.size() + 16*1024, [&] (Byte_range_ptr const &bytes) {
		if (_orig_board.size() <= bytes.num_bytes) {
			memcpy(bytes.start, _orig_board.local_addr<char>(), _orig_board.size());
			return _orig_board.size();
		}
		error("unable to import board option for editing");
		return 0ul;
	} };

	auto enable_child = [&] (auto const &name)
	{
		edit.adjust({ "option | + child ", name, " | : enabled" }, Value(),
			[&] (Value v) { return (v == "discover") ? "yes" : v; });
	};

	auto remove_child = [&] (auto const &name)
	{
		edit.remove({ "option | : child ", name });
	};

	auto enable_or_remove_child = [&] (auto const &name, bool condition)
	{
		if (condition) enable_child(name); else remove_child(name);
	};

	enable_or_remove_child("usb",       detected.usb);
	enable_or_remove_child("intel_gpu", detected.intel_gfx);
	enable_or_remove_child("intel_fb",  detected.intel_gfx &&  intel_fb_usable);
	enable_or_remove_child("boot_fb",   detected.boot_fb   && !intel_fb_usable);
	enable_or_remove_child("vesa_fb",   detected.vga && !detected.boot_fb && !intel_fb_usable);
	enable_or_remove_child("ps2",       detected.ps2);
	enable_or_remove_child("ahci",      detected.ahci);
	enable_or_remove_child("nvme",      detected.nvme);

	if (detected.boot_fb)
		Boot_fb::with_mode(platform, [&] (Boot_fb::Mode const &mode) {
			edit.adjust({ "option | + child boot_fb | : ram" }, Num_bytes(),
				[&] (Num_bytes) { return Num_bytes { mode.ram_quota().value }; }); });

	/* leave the system alone now that the detection is done */
	remove_child("discover");
	remove_child("discover_report");

	edit.with_result([&] (Span const &s) {
		Reporter reporter { _env, "option", "board", s.num_bytes };
		reporter.enabled(true);
		if (reporter.generate(s).failed())
			error("unable to deliver adjusted board option as report");
	},
	[&] (Buffer_error) { error("buffer error while adjusting board option"); },
	[&] (Alloc_error)  { error("alloc error while adjusting board option"); });

	sleep_forever();
}


void Component::construct(Genode::Env &env) { static Pc_discover::Main main(env); }

