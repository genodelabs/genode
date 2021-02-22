/*
 * \brief  I2C driver base class to be implemented by platform specific 
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-08
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _I2C_DRIVER__INTERFACE_H_
#define _I2C_DRIVER__INTERFACE_H_

/* Genode includes */
#include <base/env.h>
#include <util/xml_node.h>

namespace I2c {
	using namespace Genode;
	using Device_name = String<64>;
	class Driver_base;
}


/**
 * Base class for platform specific driver to implement
 *
 * Note about the endianess: the driver is transparent.
 *
 * The driver read/write bytes to memory in the order they are
 * read/write to the bus.
 * It is the responsability of the component interacting with
 * a slave device on the bus to figure out how to interpret the data. 
 */
struct I2c::Driver_base : Interface
{
	class Bad_bus_no: Exception {};

	/**
	 * Write to the I2C bus
	 *
	 * \param address     device address
	 * \param buffer_in   buffer containing data to be send
	 * \param buffer_size size of the buffer to be send
	 *
	 * \throw I2c::Session::Bus_error An error occured while performing an operation on the bus
	 */
	virtual void write(uint8_t address, uint8_t const *buffer_in, size_t buffer_size) = 0;

	/**
	 * Read from the I2C bus
	 *
	 * \param address     device address
	 * \param buffer_out  preallocated buffer to store the data in
	 * \param buffer_size size of the buffer and to be read
	 *
	 * \throw I2c::Session::Bus_error An error occure while performing an operation on the bus
	 */
	virtual void read(uint8_t address, uint8_t *buffer_out, size_t buffer_size) = 0;

	/**
	 * Driver name getter
	 *
	 * \return Driver name string
	 *
	 * Details this method is overridable to customise the name based on the platform.
	 */
	virtual char const *name() const { return "i2c driver"; }
};

#endif /* _I2C_INTERFACE_H_ */
