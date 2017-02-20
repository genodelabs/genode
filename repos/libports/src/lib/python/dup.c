/*
 * \brief  Libc dummies required by Python
 * \author Sebastian Sumpf
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <unistd.h>

int dup(int oldfd)
{
	return oldfd;
}

