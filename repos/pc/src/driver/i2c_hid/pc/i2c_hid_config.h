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

enum Irq_mode { GPIO, APIC };

struct i2c_hid_config
{
	enum Irq_mode mode; /* interrupt mode */
	int irq;       /* interrupt number */
	int bus_addr;  /* I2C bus slave address */
	int hid_addr;  /* HID descriptor address (I2C register) */

	char gpiochip_name[32]; /* name of GPIO chip (if mode == GPIO) */
};

extern struct i2c_hid_config i2c_hid_config;

struct i2c_master_config
{
	unsigned bus_speed_hz; /* bus speed in Hz*/
	unsigned ss_hcnt;      /* standard mode hcnt */
	unsigned ss_lcnt;      /* standard mode lcnt */
	unsigned ss_ht;        /* standard mode hold time */
	unsigned fs_hcnt;      /* fast mode hcnt */
	unsigned fs_lcnt;      /* fast mode lcnt */
	unsigned fs_ht;        /* fast mode hold time */
};

extern struct i2c_master_config i2c_master_config;

#ifdef __cplusplus
}
#endif

#endif /* _I2C_HID_CONFIG_H_ */
