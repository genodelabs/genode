/*
 * \brief  Platform driver - PCI helper utilities
 * \author Stefan Kalkowski
 * \date   2022-05-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_io_mem_dataspace.h>
#include <pci/config.h>
#include <util/reconstructible.h>

#include <device.h>
#include <device_pd.h>
#include <pci.h>
#include <pci_uhci.h>
#include <pci_intel_graphics.h>
#include <pci_hd_audio.h>
#include <pci_virtio.h>

using namespace Genode;
using namespace Pci;


struct Config_helper
{
	Driver::Device             const & _dev;
	Driver::Device::Pci_config const & _cfg;

	Attached_io_mem_dataspace _io_mem;
	Config                    _config { (addr_t)_io_mem.local_addr<void>() };

	Config_helper(Env                              & env,
	              Driver::Device             const & dev,
	              Driver::Device::Pci_config const & cfg)
	: _dev(dev), _cfg(cfg), _io_mem(env, cfg.addr, 0x1000) { }

	void enable(Driver::Device_pd & pd)
	{
		pd.assign_pci(_io_mem.cap(),
		              { _cfg.bus_num, _cfg.dev_num, _cfg.func_num });

		Config::Command::access_t cmd =
			_config.read<Config::Command>();

		/* always allow DMA operations */
		Config::Command::Bus_master_enable::set(cmd, 1);

		/* enable memory space when I/O mem is defined */
		_dev.for_each_io_mem([&] (unsigned, Driver::Device::Io_mem::Range,
		                          Driver::Device::Pci_bar) {
			Config::Command::Memory_space_enable::set(cmd, 1); });

		/* enable i/o space when I/O ports are defined */
		_dev.for_each_io_port_range(
			[&] (unsigned, Driver::Device::Io_port_range::Range) {
				Config::Command::Io_space_enable::set(cmd, 1); });

		_config.write<Config::Command>(cmd);

		/* apply different PCI quirks, bios handover etc. */
		Driver::pci_uhci_quirks(_cfg, _config.base());
		Driver::pci_hd_audio_quirks(_cfg, _config);
	}

	void disable()
	{
		Config::Command::access_t cmd =
			_config.read<Config::Command>();
		Config::Command::Io_space_enable::set(cmd, 0);
		Config::Command::Memory_space_enable::set(cmd, 0);
		Config::Command::Bus_master_enable::set(cmd, 0);
		Config::Command::Interrupt_enable::set(cmd, 0);
		_config.write<Config::Command>(cmd);
	}
};


void Driver::pci_enable(Env & env, Device_pd & pd, Device const & dev)
{
	dev.for_pci_config([&] (Device::Pci_config const & pc) {
		Config_helper(env, dev, pc).enable(pd); });
}


void Driver::pci_disable(Env & env, Device const & dev)
{
	dev.for_pci_config([&] (Device::Pci_config const & pc) {
		Config_helper(env, dev, pc).disable(); });
}


void Driver::pci_msi_enable(Env                   & env,
                            addr_t                  cfg_space,
                            Irq_session::Info const info)
{
	Attached_io_mem_dataspace io_mem { env, cfg_space, 0x1000 };
	Config config { (addr_t)io_mem.local_addr<void>() };
	config.scan();
	if (config.msi_cap.constructed())
		config.msi_cap->enable(info.address, (uint16_t)info.value);
	else error("Device does not support MSI(-x)!");
}


static inline String<16>
pci_class_code_alias(uint32_t class_code)
{
	enum { WILDCARD = 0xff };

	uint8_t  const b = (class_code >> 16) & 0xff;
	uint8_t  const s = (class_code >>  8) & 0xff;
	uint8_t  const i =  class_code        & 0xff;

	static struct Alias
	{
		String<16> name;
		uint8_t    base;
		uint8_t    sub;
		uint8_t    iface;

		bool matches(uint8_t b, uint8_t s, uint8_t i) const
		{
			return (base  == WILDCARD || base  == b) &&
			       (sub   == WILDCARD || sub   == s) &&
			       (iface == WILDCARD || iface == i);
		}
	} const aliases [] = {
		{ "NVME"     , 0x01,     0x08,     0x02     },
		{ "USB"      , 0x0c,     0x03,     0x00     }, /* UHCI */
		{ "USB"      , 0x0c,     0x03,     0x10     }, /* OHCI */
		{ "USB"      , 0x0c,     0x03,     0x20     }, /* EHCI */
		{ "USB"      , 0x0c,     0x03,     0x30     }, /* XHCI */
		{ "USB4"     , 0x0c,     0x03,     0x40     },
		{ "VGA"      , 0x03,     0x00,     0x00     },
		{ "AHCI"     , 0x01,     0x06,     WILDCARD },
		{ "AUDIO"    , 0x04,     0x01,     WILDCARD },
		{ "ETHERNET" , 0x02,     0x00,     WILDCARD },
		{ "HDAUDIO"  , 0x04,     0x03,     WILDCARD },
		{ "ISABRIDGE", 0x06,     0x01,     WILDCARD },
		{ "WIFI"     , 0x02,     0x80,     WILDCARD },
	};

	for (Alias const & alias : aliases)
		if (alias.matches(b, s, i))
			return alias.name;

	return "ALL";
}


bool Driver::pci_device_matches(Session_policy const & policy,
                                Device         const & dev)
{
	bool ret = false;

	policy.for_each_sub_node("pci", [&] (Xml_node node)
	{
		if (dev.type() != "pci")
			return;

		String<16> class_code = node.attribute_value("class", String<16>());
		vendor_t   vendor_id  = node.attribute_value<vendor_t>("vendor_id", 0);
		device_t   device_id  = node.attribute_value<device_t>("device_id", 0);

		dev.for_pci_config([&] (Device::Pci_config const cfg)
		{
			if ((pci_class_code_alias(cfg.class_code) == class_code) ||
			    (vendor_id == cfg.vendor_id && device_id == cfg.device_id))
				ret = true;
		});
	});

	return ret;
}


void Driver::pci_device_specific_info(Device const  & dev,
                                      Env           & env,
                                      Device_model  & model,
                                      Xml_generator & xml)
{
	dev.for_pci_config([&] (Device::Pci_config const cfg)
	{
		Driver::pci_intel_graphics_info(cfg, env, model, xml);
		Driver::pci_virtio_info(dev, cfg, env, xml);
	});
}
