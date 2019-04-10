/*
 * \brief  Test for validating the device detection of the driver manager
 * \author Norman Feske
 * \date   2017-06-30
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/allocator_avl.h>
#include <base/heap.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <block_session/connection.h>

namespace Test {
	using namespace Genode;
	struct Main;
}


struct Test::Main
{
	Env &_env;

	Attached_rom_dataspace _config        { _env, "config" };
	Attached_rom_dataspace _block_devices { _env, "block_devices" };

	static bool _attribute_matches(char const *attr, Xml_node expected, Xml_node checked)
	{
		typedef String<80> Value;
		return !expected.has_attribute(attr)
		    || (expected.attribute_value(attr, Value()) ==
		        checked .attribute_value(attr, Value()));
	}

	static bool _block_device_matches(Xml_node expect, Xml_node device)
	{
		return _attribute_matches("label",       expect, device)
		    && _attribute_matches("block_size",  expect, device)
		    && _attribute_matches("block_count", expect, device);
	}

	static bool _usb_block_device_matches(Xml_node expect, Xml_node device)
	{
		return _block_device_matches(expect, device)
		    && _attribute_matches("vendor",  expect, device)
		    && _attribute_matches("product", expect, device);
	}

	static bool _ahci_block_device_matches(Xml_node expect, Xml_node device)
	{
		return _block_device_matches(expect, device)
		    && _attribute_matches("model",  expect, device)
		    && _attribute_matches("serial", expect, device);
	}

	void _check_conditions()
	{
		_block_devices.update();

		bool expected_devices_present = true;

		log("-- check presence of expected block devices --");

		_config.xml().for_each_sub_node([&] (Xml_node expect) {

			/* skip nodes that are unrelated to block devices */
			if (expect.type() != "check_usb_block_device"
			 && expect.type() != "check_ahci_block_device")
				return;

			bool device_exists = false;

			_block_devices.xml().for_each_sub_node("device", [&] (Xml_node device) {

				if (expect.type() == "check_usb_block_device"
				 && _usb_block_device_matches(expect, device))
					device_exists = true;

				if (expect.type() == "check_ahci_block_device"
				 && _ahci_block_device_matches(expect, device))
					device_exists = true;
			});

			log("block device '", expect.attribute_value("label", String<80>()), "' ",
			    device_exists ? "present" : "not present");

			if (!device_exists)
				expected_devices_present = false;
		});

		if (!expected_devices_present)
			return;

		/* attempt to create a session to each block device */
		_block_devices.xml().for_each_sub_node("device", [&] (Xml_node device) {

			typedef String<64> Label;
			Label label = device.attribute_value("label", Label());

			log("connect to block device '", label, "'");

			Heap heap(_env.ram(), _env.rm());
			Allocator_avl packet_alloc(&heap);
			Block::Connection<> block(_env, &packet_alloc, 128*1024, label.string());
		});

		log("all expected devices present and accessible");
	}

	Signal_handler<Main> _block_devices_update_handler {
		_env.ep(), *this, &Main::_check_conditions };

	Main(Env &env) : _env(env)
	{
		if (_config.xml().has_sub_node("check_framebuffer")) {
			log("connect to framebuffer driver");
			Framebuffer::Mode mode(640, 480, Framebuffer::Mode::RGB565);
			Framebuffer::Connection fb(_env, mode);
		}

		if (_config.xml().has_sub_node("check_input")) {
			log("connect to input driver");
			Input::Connection input(_env);
		}

		_block_devices.sigh(_block_devices_update_handler);
		_check_conditions();
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

