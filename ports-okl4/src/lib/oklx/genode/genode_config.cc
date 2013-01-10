/*
 * \brief  Genode C API config functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 *
 * OKLinux includes several stub drivers, used as frontends to the Genode
 * framework, e.g. framebuffer-driver or rom-file block-driver.
 * Due to the strong bond of these drivers with the Genode framework,
 * they should be configured using Genode's configuration format and
 * not the kernel commandline.
 * A valid configuration example can be found in 'config/init_config'
 * within this repository.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <os/config.h>

#include <oklx_screens.h>


static Genode::Xml_node node(const char* name)
{
	Genode::Xml_node config_node = Genode::config()->xml_node();

	for (unsigned i = 0; i < config_node.num_sub_nodes(); i++) {
		Genode::Xml_node node = config_node.sub_node(i);
		if(node.has_type(name))
			return node;
	}
	throw Genode::Xml_node::Nonexistent_sub_node();
}


static void content(const char* node_name, const char *attr_name,
                    char *buf, Genode::size_t sz)
{
	*buf = 0;
	try {
		Genode::Xml_node _node = node(node_name);
		_node.attribute(attr_name).value(buf, sz);
	} catch (Genode::Xml_node::Invalid_syntax) {
		PWRN("Malformed entry in Linux config.");
	} catch (...) { }
}


Screen_array::Screen_array()
{
	try {
		Genode::Xml_node scr_node = node("screens");
		for (unsigned i = 0; i < SIZE; i++) {
			if (i >= scr_node.num_sub_nodes())
				_screens[i] = (Screen*) 0;
			else {
				Genode::Xml_node n = scr_node.sub_node(i);
				if(n.has_type("framebuffer"))
					_screens[i] = new (Genode::env()->heap()) Simple_screen();
				else if(n.has_type("nitpicker"))
					_screens[i] = new (Genode::env()->heap()) Nitpicker_screen();
				else
					PWRN("Ignoring unknown tag in screen section");
			}
		}
	} catch (Genode::Xml_node::Invalid_syntax) {
		PWRN("Malformed entry in Linux config.");
	} catch (Genode::Xml_node::Nonexistent_sub_node) {
		PWRN("No screen section in config");
	}
}


extern "C" {
#include <genode/config.h>

	char* genode_config_cmdline()
	{
		static char cmd_line[512];
		static bool initialized = false;
		if (!initialized) {
			content("commandline", "args", cmd_line, sizeof(cmd_line));
			initialized = true;
		}
		return Genode::strlen(cmd_line) ? cmd_line : 0;
	}


	char* genode_config_initrd()
	{
		static char initrd[64];
		static bool initialized = false;
		if (!initialized) {
			content("initrd", "name", initrd, sizeof(initrd));
			initialized = true;
		}
		return Genode::strlen(initrd) ? initrd : 0;
	}


	int genode_config_audio()
	{
		try {
			node("audio");
			return 1;
		} catch (Genode::Xml_node::Invalid_syntax) {
			PWRN("Malformed entry in Linux config.");
		} catch (Genode::Xml_node::Nonexistent_sub_node) {
		}
		return 0;
	}


	int genode_config_nic()
	{
		try {
			node("nic");
			return 1;
		} catch (Genode::Xml_node::Invalid_syntax) {
			PWRN("Malformed entry in Linux config.");
		} catch (Genode::Xml_node::Nonexistent_sub_node) {
		}
		return 0;
	}

	int genode_config_block()
	{
		try {
			node("block");
			return 1;
		} catch (Genode::Xml_node::Invalid_syntax) {
			PWRN("Malformed entry in Linux config.");
		} catch (Genode::Xml_node::Nonexistent_sub_node) {
		}
		return 0;
	}

} // extern "C"
