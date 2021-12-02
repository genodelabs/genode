/*
 * \brief  Test platform driver API
 * \author Stefan Kalkowski
 * \date   2021-12-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <os/reporter.h>

/* WARNING DO NOT COPY THIS !!! */
/*
 * We make everything public from the platform device classes
 * to be able to check all corner cases while stressing the API
 */
#define private public
#include <platform_session/device.h>
#undef  private
/* WARNING DO NOT COPY THIS !!! */

using namespace Genode;

struct Main
{
	struct Device
	{
		Constructible<Platform::Device>       device {};
		Constructible<Platform::Device::Mmio> mmio {};
		Constructible<Platform::Device::Irq>  irq {};

		Device(Platform::Connection & plat, Platform::Device::Name name)
		{
			device.construct(plat, name);
			if (!device->_cap.valid()) {
				error("Device ", name, " not valid!");
				return;
			}

			mmio.construct(*device, Platform::Device::Mmio::Index{0});
			irq.construct(*device, Platform::Device::Irq::Index{0});
		}

		~Device()
		{
			if (mmio.constructed())   mmio.destruct();
			if (irq.constructed())    irq.destruct();
			if (device.constructed()) device.destruct();
		}
	};

	Env      & env;
	Reporter   config_reporter { env, "config" };

	Reconstructible<Platform::Connection> platform { env };
	Constructible<Platform::Connection>   platform_2 { };

	Signal_handler<Main> device_rom_handler { env.ep(), *this,
	                                          &Main::handle_device_update };

	Constructible<Device> devices[4] {};
	unsigned              state = 0;

	void report_devices(unsigned assigned,   unsigned total,
	                    addr_t   iomem_base, unsigned irq_base)
	{
		Reporter::Xml_generator xml(config_reporter, [&] ()
		{
			for (unsigned idx = 0; idx < total; idx++) {
				xml.node("device", [&]
				{
					xml.attribute("name", idx);
					xml.attribute("type", "dummy-device");
					xml.node("io_mem", [&]
					{
						xml.attribute("address",
									  String<16>(Hex(iomem_base + idx*0x1000UL)));
						xml.attribute("size", String<16>(Hex(0x1000UL)));
					});
					xml.node("irq", [&]
					{
						xml.attribute("number", irq_base + idx);
					});
				});
			}

			xml.node("policy", [&]
			{
				xml.attribute("label", "test-platform_drv -> ");
				xml.attribute("info", true);

				for (unsigned idx = 0; idx < assigned; idx++) {
					xml.node("device", [&]
					{
						xml.attribute("name", idx);
					});
				};
			});
		});
	}

	void start_driver(unsigned idx)
	{
		if (!devices[idx].constructed())
			devices[idx].construct(*platform, Platform::Device::Name(idx));
	}

	void stop_driver(unsigned idx)
	{
		if (devices[idx].constructed())
			devices[idx].destruct();
	}

	void step()
	{
		switch (state++) {
		case 0:
			/* report 3 out of 6 devices */
			report_devices(3, 6, 0x40000000, 32);
			return;
		case 1:
			/* start all drivers for the 3 devices, then destroy one, and let it vanish */
			start_driver(0);
			start_driver(1);
			start_driver(2);
			stop_driver(2);
			report_devices(2, 2, 0x40000000, 32);
			return;
		case 2:
			/* repeatedly start and destroy device sessions to detect leakages */
			for (unsigned idx = 0; idx < 100; idx++) {
				start_driver(0);
				start_driver(1);
				stop_driver(0);
				stop_driver(1);
			}
			/* now let all devices vanish */
			report_devices(0, 2, 0x40000000, 32);
			return;
		case 3:
			report_devices(4, 4, 0x40000000, 32);
			return;
		case 4:
			/* Instantiate and destroy all devices */
			start_driver(0);
			start_driver(1);
			start_driver(2);
			start_driver(3);
			stop_driver(0);
			stop_driver(1);
			stop_driver(2);
			stop_driver(3);
			/* allocate big DMA dataspace */
			platform->alloc_dma_buffer(0x80000, UNCACHED);
			/* close the whole session */
			platform.destruct();
			platform.construct(env);
			platform->sigh(device_rom_handler);
			start_driver(0);
			start_driver(1);
			start_driver(2);
			start_driver(3);
			/* repeatedly start and destroy device sessions to detect leakages */
			for (unsigned idx = 0; idx < 1000; idx++) {
				platform->free_dma_buffer(platform->alloc_dma_buffer(0x4000, UNCACHED));
			}
			report_devices(0, 0, 0x40000000, 32);
			return;
		case 5:
			stop_driver(0);
			stop_driver(1);
			stop_driver(2);
			stop_driver(3);
			report_devices(1, 1, 0x40000100, 32);
			return;
		case 6:
			start_driver(0);
			platform_2.construct(env);
			devices[1].construct(*platform_2, Platform::Device::Name(0));
			stop_driver(1);
			platform_2.destruct();
			report_devices(4, 4, 0x40000100, 32);
			return;
		case 7:
			{
				Platform::Device dev (*platform, Platform::Device::Type({"dummy-device"}));
				if (dev._cap.valid()) { log("Found next valid device of dummy type"); }
				Reporter::Xml_generator xml(config_reporter, [&] () {});
				break;
			}
		default:
			error("Invalid state something went wrong");
		};

		log("Test has ended!");
	}

	void handle_device_update()
	{
		platform->update();
		platform->with_xml([&] (Xml_node & xml) { log(xml); });
		step();
	}

	Main(Env &env) : env(env)
	{
		platform->sigh(device_rom_handler);
		config_reporter.enabled(true);
		step();
	}
};

void Component::construct(Genode::Env &env) { static Main main(env); }
