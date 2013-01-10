/*
 * \brief  fcntl.h prototypes/definitions required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FCNTL_H_
#define _FCNTL_H_

enum flags {
	O_RDONLY = 0x0
};

int open(const char *pathname, int flags);

#endif //_FCNTL_H_
