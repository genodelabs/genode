/*
 * \brief  'main()' arguments test
 * \author Christian Prochaska
 * \date   2012-04-19
 *
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <util/string.h>

using namespace Genode;

int main(int argc, char **argv)
{
	printf("--- config args test started ---\n");

	if (argc != 2) {
		PERR("Error: argc is not as expected");
		return -1;
	}

	if (strcmp(argv[0], "test-config_args") != 0) {
		PERR("Error: argv[0] is not as expected");
		return -1;
	}

	if (strcmp(argv[1], "-testarg") != 0) {
		PERR("Error: argv[1] is not as expected");
		return -1;
	}

	printf("--- end of config args test ---\n");

	return 0;
}
