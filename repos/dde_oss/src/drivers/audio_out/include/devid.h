/*
 * \brief  Operating system specific device id structure
 * \author Sebastian Sumpf
 * \date   2012-11-20
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DEVID_H_
#define _INCLUDE__DEVID_H_

typedef struct
{
	unsigned short vendor;
	unsigned short product;
} device_id_t;

#endif /* _INCLUDE__DEVID_H_ */
