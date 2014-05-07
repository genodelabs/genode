/*
 * \brief  Driver quirks interface
 * \author Sebastian Sumpf
 * \date   2012-11-27
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__QUIRKS_H_
#define _INCLUDE__QUIRKS_H_

struct oss_driver;

/**
 * Check and possibly set quirks for given driver
 */
void setup_quirks(struct oss_driver *drv);

#endif /* _INCLUDE__QUIRKS_H_ */
