/*
 * \brief  Regulator-driver interface
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGULATOR__DRIVER_H_
#define _INCLUDE__REGULATOR__DRIVER_H_

#include <regulator/consts.h>

namespace Regulator {

	struct Driver;
	struct Driver_factory;
}


/**
 * Interface to be implemented by the device-specific driver code
 */
struct Regulator::Driver
{
	virtual void level(Regulator_id id, unsigned long level) = 0;
	virtual unsigned long level(Regulator_id id)             = 0;
	virtual void state(Regulator_id id, bool enable)         = 0;
	virtual bool state(Regulator_id id)                      = 0;
};


/**
 * Interface for constructing the driver object
 */
struct Regulator::Driver_factory
{
	/**
	 * Construct new driver
	 */
	virtual Driver &create(Regulator_id regulator) = 0;

	/**
	 * Destroy driver
	 */
	virtual void destroy(Driver &driver) = 0;
};

#endif /* _REGULATOR__DRIVER_H_ */
