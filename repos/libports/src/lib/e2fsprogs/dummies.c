/*
 * \brief  Dummies to prevent unneeded warnings
 * \author Josef Soentgen
 * \date   2022-06-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>


int getrusage(int who, struct rusage *usage)
{
	(void)who;
	(void)usage;
	return -1;
}


void *sbrk(intptr_t increment)
{
	(void)increment;
	return (void*)0;
}
