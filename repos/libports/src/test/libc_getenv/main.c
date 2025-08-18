/*
 * \brief  Libc getenv(...) test
 * \author Emery Hemingway
 * \date   2017-01-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; ++i) {
		char const *key = argv[i];
		if (getenv(key))
			printf("%s=\"%s\"\n", key, getenv(key));
		else
			printf("no env variable named '%s'\n", key);
	}

	return 0;
}
