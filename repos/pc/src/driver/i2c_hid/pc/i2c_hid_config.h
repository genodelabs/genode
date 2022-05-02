/**
 * \brief  Configuration attributes for i2c_hid
 * \author Christian Helmuth
 * \date   2024-04-22
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _I2C_HID_CONFIG_H_
#define _I2C_HID_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

struct i2c_hid_config
{
	int gpio_pin; /* GPIO touchpad interrupt pin */
	int bus_addr; /* I2C bus slave address */
	int hid_addr; /* HID descriptor address (I2C register) */
};

extern struct i2c_hid_config i2c_hid_config;

#ifdef __cplusplus
}
#endif

#endif /* _I2C_HID_CONFIG_H_ */
