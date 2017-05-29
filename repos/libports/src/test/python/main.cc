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

/* libc includes */
#include <fcntl.h>


extern "C"  int __sread(void *, char *, int);


int main(int argc, char const ** args)
{
	using namespace Genode;

	if (argc < 1) {
		Genode::error("Need <scriptname>.py as argument!");
		return -1;
	}

	char * name = const_cast<char*>(args[0]);
	FILE * fp = fopen(name, "r");
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
