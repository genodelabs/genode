/*
 * \brief  Firmware I/O functions
 * \author Josef Soentgen
 * \date   2023-05-05
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ACCESS_FIRMWARE_H_
#define _ACCESS_FIRMWARE_H_

struct Stat_firmware_result
{
	size_t length;
	bool   success;
};

Stat_firmware_result access_firmware(char const *path);


struct Read_firmware_result
{
	bool success;
};

Read_firmware_result read_firmware(char const *path, char *dst, size_t dst_len);

#endif /* _ACCESS_FIRMWARE_H_ */
