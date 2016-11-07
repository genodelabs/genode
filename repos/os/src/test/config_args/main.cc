/*
 * \brief  'main()' arguments test
 * \author Christian Prochaska
 * \date   2012-04-19
 *
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <util/string.h>

using namespace Genode;

int main(int argc, char **argv)
{
	log("--- config args test started ---");

	if (argc != 2) {
		error("argc is not as expected");
		return -1;
	}

	if (strcmp(argv[0], "test-config_args") != 0) {
		error("argv[0] is not as expected");
		return -1;
	}

	if (strcmp(argv[1], "-testarg") != 0) {
		error("argv[1] is not as expected");
		return -1;
	}

	log("--- end of config args test ---");

	return 0;
}
