/*
 * \brief  Print counter every second
 * \author Norman Feske
 * \date   2023-03-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	for (unsigned count = 0; ; count++) {
		printf("%d\n", count);
		sleep(1);
	}
}
