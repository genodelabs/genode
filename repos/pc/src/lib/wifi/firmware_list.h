/*
 * \brief  List for firmware images and their sizes
 * \author Josef Soentgen
 * \date   2014-03-26
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _FIRMWARE_LIST_H_
#define _FIRMWARE_LIST_H_

typedef __SIZE_TYPE__ size_t;

struct Firmware_list
{
	char const *requested_name;
	size_t      size;
	char const *available_name;
};

#endif /* _FIRMWARE_LIST_H_ */
