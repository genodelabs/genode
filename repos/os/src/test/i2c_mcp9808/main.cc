/*
 * \brief  Test I2C driver with MCP9808 sensor
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-26
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <i2c_session/connection.h>

namespace Test {
	using namespace Genode;
	struct Main;
}


struct Test::Main
{
	Env &_env;

	I2c::Connection _sensor { _env, "MCP_9808" };

	Main(Env &env) : _env(env)
	{
		uint16_t raw_data = 0;

		try{
			/* Config ambient mode */
			_sensor.write_8bits(0x05);

			/* Read ambient temperature */
			raw_data = _sensor.read_16bits();
		} catch (I2c::Session::Bus_error) {
			error("Bus operation could not be completed.");
			return;
		}

		uint8_t* temp_data = reinterpret_cast<uint8_t*>(&raw_data);

		/* Convert the temperature data */
		/* Check flag bits */
		if ((temp_data[0] & 0x80) == 0x80) {
			/* T A is T CRIT */
			warning("Temperature is critical for the sensor.");
		}
		if ((temp_data[0] & 0x40) == 0x40) {
			/* T A > T UPPER */
			warning("Temperature is upper the bound of the sensor.");
		}
		if ((temp_data[0] & 0x20) == 0x20) {
			/* T A < T LOWER */
			warning("Temperature is lower the bound of the sensor.");
		}

		/* Clear the flag's bits */
		temp_data[0] = temp_data[0] & 0x1F;
		int temperature = 0;

		/* T A is negative */
		if ((temp_data[0] & 0x10) == 0x10){
			/* Clear SIGN */
			temp_data[0] = temp_data[0] & 0x0F;
			temperature = 256 - (temp_data[0] * 16 + temp_data[1] / 16);
		} else {
			/* T A is positive */
			temperature = (temp_data[0] * 16 + temp_data [1] / 16);
		}

		log("temperature is ", temperature, " C");
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }
