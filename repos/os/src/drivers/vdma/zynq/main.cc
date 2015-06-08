/*
 * \brief  VDMA Driver for Zynq
 * \author Mark Albers
 * \date   2015-04-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <vdma_session/zynq/vdma_session.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <root/component.h>
#include <os/static_root.h>
#include <os/config.h>
#include <vector>
#include "driver.h"

namespace Vdma {
	using namespace Genode;
	class Session_component;
	class Root;
};

class Vdma::Session_component : public Genode::Rpc_object<Vdma::Session>
{
	private:

		Driver &_driver;
        unsigned _number;

	public:

        Session_component(Driver &driver, unsigned vdma_number) : _driver(driver), _number(vdma_number) {}

        virtual bool setConfig(Genode::uint32_t data, bool isMM2S)
        {
            return _driver.setConfig(_number, data, isMM2S);
        }

        virtual bool setStride(Genode::uint16_t data, bool isMM2S)
        {
            return _driver.setStride(_number, data, isMM2S);
        }

        virtual bool setWidth(Genode::uint16_t data, bool isMM2S)
        {
            return _driver.setWidth(_number, data, isMM2S);
        }

        virtual bool setHeight(Genode::uint16_t data, bool isMM2S)
        {
            return _driver.setHeight(_number, data, isMM2S);
        }

        virtual bool setAddr(Genode::uint32_t data, bool isMM2S)
        {
            return _driver.setAddr(_number, data, isMM2S);
        }
};

class Vdma::Root : public Genode::Root_component<Vdma::Session_component>
{
	private:

		Driver &_driver;

	protected:

		Session_component *_create_session(const char *args)
		{
            unsigned number = Genode::Arg_string::find_arg(args, "vdma").ulong_value(0);
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
        : Genode::Root_component<Vdma::Session_component>(session_ep, md_alloc),
		  _driver(driver) { }
};

int main(int, char **)
{
    using namespace Vdma;

    PINF("Zynq VDMA driver");

    /*
     * Read config
     */
    std::vector<Genode::addr_t> addr;

    try {
        Genode::Xml_node vdma_node = Genode::config()->xml_node().sub_node("vdma");

        for (unsigned i = 0; ;i++, vdma_node = vdma_node.next("vdma"))
        {
            addr.push_back(0);
            vdma_node.attribute("addr").value(&addr[i]);
            PINF("VDMA with mio address 0x%x added.", (unsigned int)addr[i]);

            if (vdma_node.is_last("vdma")) break;
        }
    }
    catch (Genode::Xml_node::Nonexistent_sub_node) {
        PWRN("No VDMA config");
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
    static Rpc_entrypoint ep(&cap, STACK_SIZE, "vdma_ep");
    static Vdma::Root vdma_root(&ep, &sliced_heap, driver);

	/*
	 * Announce service
     */
    env()->parent()->announce(ep.manage(&vdma_root));

	Genode::sleep_forever();
	return 0;
}
