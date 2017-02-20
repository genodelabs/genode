/*
 * \brief  Configuration of the Driver for the Central Security Unit
 * \author Martin Stein
 * \date   2015-10-30
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__USB_ARMORY__CSU_CONFIG_H_
#define _CORE__INCLUDE__SPEC__USB_ARMORY__CSU_CONFIG_H_

/**
 * Configuration of the Driver for the Central Security Unit
 */
namespace Csu_config
{
	enum {
		SECURE_GPIO  = 0,
		SECURE_ESDHC = 1,
		SECURE_UART  = 1,
		SECURE_I2C   = 0,
	};
};

#endif /* _CORE__INCLUDE__SPEC__USB_ARMORY__CSU_CONFIG_H_ */
