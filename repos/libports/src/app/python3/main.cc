/*
 * \brief  Running python script on Genode
 * \author Johannes Schlatow
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Python includes */
#include <python3/Python.h>

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <libc/component.h>
#include <base/log.h>

/* libc includes */
#include <fcntl.h>

namespace Python
{
	struct Main;
}

struct Python::Main
{
	Genode::Env       &_env;

	Genode::Attached_rom_dataspace _config = { _env, "config" };

	void _handle_config()
	{
		enum {
			MAX_NAME_LEN = 128
		};

		char filename[MAX_NAME_LEN];

		if (_config.xml().has_sub_node("file")) {
			Genode::Xml_node script = _config.xml().sub_node("file");

			script.attribute("name").value(filename, sizeof(filename));
		} else {
			Genode::error("Need <file name=\"filename\"> as argument!");
			return;
		}

		wchar_t wbuf[MAX_NAME_LEN];

		if (_config.xml().has_sub_node("pythonpath")) {
			char pythonpath[MAX_NAME_LEN];
			Genode::Xml_node path = _config.xml().sub_node("pythonpath");

			path.attribute("name").value(pythonpath, sizeof(pythonpath));
			mbstowcs(wbuf, pythonpath, strlen(pythonpath));

			Py_SetPath(wbuf);
		}

		mbstowcs(wbuf, filename, strlen(filename));

		FILE * fp = fopen(filename, "r");
		//fp._flags = __SRD;
		Py_SetProgramName(wbuf);
		//don't need the 'site' module
		Py_NoSiteFlag = 1;
		//don't support interactive mode, yet
		Py_InteractiveFlag = 0;
		Py_Initialize();

		Genode::log("Starting python ...");
		PyRun_SimpleFile(fp, filename);
		Genode::log("Executed '", Genode::Cstring(filename), "'");
		Py_Finalize();
	}

	Genode::Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Main(Genode::Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] () { static Python::Main main(env); });
}
