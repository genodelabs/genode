/*
 * \brief  I2C Driver for Zynq
 * \author Mark Albers
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <i2c_session/zynq/i2c_session.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <root/component.h>

#include <os/static_root.h>
#include <os/config.h>

#include "driver.h"

namespace I2C {
	using namespace Genode;
	class Session_component;
	class Root;
};

class I2C::Session_component : public Genode::Rpc_object<I2C::Session>
{
	private:
		Driver &_driver;
		unsigned int _bus;
		Genode::Signal_context_capability _sigh;

	public:

		Session_component(Driver &driver, unsigned int bus_num) : _driver(driver), _bus(bus_num) {}

		virtual bool read_byte_16bit_reg(Genode::uint8_t adr, Genode::uint16_t reg, Genode::uint8_t *data)
		{
			return _driver.read_byte_16bit_reg(_bus, adr, reg, data);
		}

		virtual bool write_16bit_reg(Genode::uint8_t adr, Genode::uint16_t reg,
			Genode::uint8_t data)
		{
			return _driver.write_16bit_reg(_bus, adr, reg, data);
		}
};

class I2C::Root : public Genode::Root_component<I2C::Session_component>
{
	private:

		Driver &_driver;

	protected:

		Session_component *_create_session(const char *args)
		{
			unsigned int bus = Genode::Arg_string::find_arg(args, "bus").ulong_value(0);
			return new (md_alloc()) Session_component(_driver, bus);
		}

	public:

		Root(Genode::Rpc_entrypoint *session_ep,
		     Genode::Allocator *md_alloc, Driver &driver)
		: Genode::Root_component<I2C::Session_component>(session_ep, md_alloc),
		  _driver(driver) { }
};

int main(int, char **)
{
	using namespace I2C;

	PINF("Zynq I2C driver");

	Driver &driver = Driver::factory();

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "i2c_ep");
	static I2C::Root i2c_root(&ep, &sliced_heap, driver);

	/*
	 * Announce service
	 */
	env()->parent()->announce(ep.manage(&i2c_root));

	Genode::sleep_forever();
	return 0;
}

