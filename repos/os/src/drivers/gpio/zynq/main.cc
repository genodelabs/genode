/*
 * \brief  Gpio Driver for Zynq
 * \author Mark Albers
 * \date   2015-03-30
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <gpio_session/zynq/gpio_session.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <root/component.h>
#include <os/static_root.h>
#include <os/config.h>
#include <vector>
#include "driver.h"

namespace Gpio {
	using namespace Genode;
	class Session_component;
	class Root;
};

class Gpio::Session_component : public Genode::Rpc_object<Gpio::Session>
{
	private:

		Driver &_driver;
        unsigned _number;

	public:

        Session_component(Driver &driver, unsigned gpio_number) : _driver(driver), _number(gpio_number) {}

        virtual Genode::uint32_t read(bool isChannel2)
		{
            return _driver.read(_number, isChannel2);
		}

        virtual bool write(Genode::uint32_t data, bool isChannel2)
		{
            return _driver.write(_number, data, isChannel2);
		}
};

class Gpio::Root : public Genode::Root_component<Gpio::Session_component>
{
	private:

		Driver &_driver;

	protected:

		Session_component *_create_session(const char *args)
		{
            unsigned number = Genode::Arg_string::find_arg(args, "gpio").ulong_value(0);
            Genode::size_t ram_quota = Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);

            if (ram_quota < sizeof(Session_component)) {
                PWRN("Insufficient dontated ram_quota (%zd bytes), require %zd bytes",
                     ram_quota, sizeof(Session_component));
                throw Genode::Root::Quota_exceeded();
            }

            return new (md_alloc()) Session_component(_driver, number);
		}

	public:

		Root(Genode::Rpc_entrypoint *session_ep,
		     Genode::Allocator *md_alloc, Driver &driver)
        : Genode::Root_component<Gpio::Session_component>(session_ep, md_alloc),
		  _driver(driver) { }
};

int main(int, char **)
{
    using namespace Gpio;

    PINF("Zynq Gpio driver");

    /*
     * Read config
     */
    std::vector<Genode::addr_t> addr;

    try {
        Genode::Xml_node gpio_node = Genode::config()->xml_node().sub_node("gpio");

        for (unsigned i = 0; ;i++, gpio_node = gpio_node.next("gpio"))
        {
            addr.push_back(0);
            gpio_node.attribute("addr").value(&addr[i]);
            PINF("Gpio with mio address 0x%x added.", (unsigned int)addr[i]);

            if (gpio_node.is_last("gpio")) break;
        }
    }
    catch (Genode::Xml_node::Nonexistent_sub_node) {
        PWRN("No Gpio config");
    }

    /*
     * Create Driver
     */
    Driver &driver = Driver::factory(addr);
    addr.clear();

	/*
	 * Initialize server entry point
	 */
    enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());
    static Rpc_entrypoint ep(&cap, STACK_SIZE, "gpio_ep");
    static Gpio::Root gpio_root(&ep, &sliced_heap, driver);

	/*
	 * Announce service
     */
    env()->parent()->announce(ep.manage(&gpio_root));

	Genode::sleep_forever();
	return 0;
}
