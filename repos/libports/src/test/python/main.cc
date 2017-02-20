/*
 * \brief  Test for using Python on Genode
 * \author Sebastian Sumpf
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Python includes */
#include <python2.6/Python.h>

/* Genode includes */
#include <base/log.h>
#include <os/config.h>

/* libc includes */
#include <fcntl.h>


extern "C"  int __sread(void *, char *, int);

static bool process_config(char **file)
{
	using namespace Genode;
	static char file_name[64];

	try {
		Xml_node config_node = config()->xml_node();
		Xml_node script_node = config_node.sub_node("script");
		script_node.attribute("name").value(file_name, sizeof(file_name));
		*file = file_name;
		return true;
	}
	catch (Xml_node::Nonexistent_sub_node) {
		Genode::error("no 'config/script' sub node in config found"); }
	catch (Xml_node::Nonexistent_attribute) {
		Genode::error("no 'name' attribute in 'script' node found"); }

	return false;
}


int main()
{
	using namespace Genode;

	char *name;
	if (!process_config(&name)) {
		Genode::error("no script found");
		return 1;
	}

	Genode::log("Found script: ", Genode::Cstring(name));
	FILE* fp = fopen(name, "r");
	//fp._flags = __SRD;
	Py_SetProgramName(name);
	//don't need the 'site' module
	Py_NoSiteFlag = 1;
	//don't support interactive mode, yet
	Py_InteractiveFlag = 0;
	Py_Initialize();

	Genode::log("Starting python ...");
	PyRun_SimpleFile(fp, name);

	return 0;
}
