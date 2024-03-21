/*
 * \brief  Driver manager
 * \author Norman Feske
 * \date   2017-06-13
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/registry.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <block_session/block_session.h>
#include <rm_session/rm_session.h>
#include <capture_session/capture_session.h>
#include <gpu_session/gpu_session.h>
#include <io_mem_session/io_mem_session.h>
#include <io_port_session/io_port_session.h>
#include <timer_session/timer_session.h>
#include <log_session/log_session.h>
#include <platform_session/platform_session.h>

namespace Driver_manager {
	using namespace Genode;
	struct Main;
	struct Device_driver;
	struct Nvme_driver;

	struct Priority { int value; };

	struct Version { unsigned value; };
}


class Driver_manager::Device_driver : Noncopyable
{
	public:

		typedef String<64>  Name;
		typedef String<100> Binary;
		typedef String<32>  Service;

	protected:

		static void _gen_common_start_node_content(Xml_generator &xml,
		                                           Name    const &name,
		                                           Binary  const &binary,
		                                           Ram_quota      ram,
		                                           Cap_quota      caps,
		                                           Priority       priority,
		                                           Version        version)
		{
			xml.attribute("name", name);
			xml.attribute("caps", String<64>(caps));
			xml.attribute("priority", priority.value);
			xml.attribute("version", version.value);
			xml.node("binary", [&] () { xml.attribute("name", binary); });
			xml.node("resource", [&] () {
				xml.attribute("name", "RAM");
				xml.attribute("quantum", String<64>(ram));
			});
		}

		template <typename SESSION>
		static void _gen_provides_node(Xml_generator &xml)
		{
			xml.node("provides", [&] () {
				xml.node("service", [&] () {
					xml.attribute("name", SESSION::service_name()); }); });
		}

		static void _gen_config_route(Xml_generator &xml, char const *config_name)
		{
			xml.node("service", [&] () {
				xml.attribute("name", Rom_session::service_name());
				xml.attribute("label", "config");
				xml.node("parent", [&] () {
					xml.attribute("label", config_name); });
			});
		}

		static void _gen_default_parent_route(Xml_generator &xml)
		{
			xml.node("any-service", [&] () {
				xml.node("parent", [&] () { }); });
		}

		template <typename SESSION>
		static void _gen_forwarded_service(Xml_generator &xml,
		                                   Device_driver::Name const &name)
		{
			xml.node("service", [&] () {
				xml.attribute("name", SESSION::service_name());
				xml.node("default-policy", [&] () {
					xml.node("child", [&] () {
						xml.attribute("name", name);
					});
				});
			});
		};

		virtual ~Device_driver() { }

	public:

		virtual void generate_start_node(Xml_generator &xml) const = 0;
};


struct Driver_manager::Nvme_driver : Device_driver
{
	void generate_start_node(Xml_generator &xml) const override
	{
		xml.node("start", [&] () {
			_gen_common_start_node_content(xml, "nvme_drv", "nvme_drv",
			                               Ram_quota{8*1024*1024}, Cap_quota{100},
			                               Priority{-1}, Version{0});
			_gen_provides_node<Block::Session>(xml);
			xml.node("config", [&] () {
				xml.node("report", [&] () { xml.attribute("namespaces", "yes"); });
				xml.node("policy", [&] () {
					xml.attribute("label_suffix", String<64>("nvme-0"));
					xml.attribute("namespace", 1);
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("route", [&] () {
				xml.node("service", [&] () {
					xml.attribute("name", "Report");
					xml.node("parent", [&] () { xml.attribute("label", "nvme_ns"); });
				});
				_gen_default_parent_route(xml);
			});
		});
	}

	typedef String<32> Default_label;

	void gen_service_forwarding_policy(Xml_generator &xml,
	                                   Default_label const &default_label) const
	{
		xml.node("policy", [&] () {
			xml.attribute("label_suffix", String<64>("nvme-0"));
			xml.node("child", [&] () {
				xml.attribute("name", "nvme_drv"); });
		});

		if (default_label.valid()) {
			xml.node("policy", [&] () {
				xml.attribute("label_suffix", " default");
				xml.node("child", [&] () {
					xml.attribute("name", "nvme_drv");
					xml.attribute("label", default_label);
				});
			});
		}
	}
};


struct Driver_manager::Main
{
	Env &_env;

	Attached_rom_dataspace _platform    { _env, "platform_info" };
	Attached_rom_dataspace _devices     { _env, "devices"   };
	Attached_rom_dataspace _nvme_ns     { _env, "nvme_ns"       };

	Reporter _init_config    { _env, "config", "init.config" };
	Reporter _block_devices  { _env, "block_devices" };

	Constructible<Nvme_driver> _nvme_driver     { };

	bool _devices_rom_parsed { false };

	void _handle_devices_update();

	Signal_handler<Main> _devices_update_handler {
		_env.ep(), *this, &Main::_handle_devices_update };

	void _handle_nvme_ns_update();

	Signal_handler<Main> _nvme_ns_update_handler {
		_env.ep(), *this, &Main::_handle_nvme_ns_update };

	static void _gen_parent_service_xml(Xml_generator &xml, char const *name)
	{
		xml.node("service", [&] () { xml.attribute("name", name); });
	};

	void _generate_init_config  (Reporter &) const;
	void _generate_block_devices(Reporter &) const;

	void _generate_block_devices()
	{
		/* devices must be detected before the checks below can be conducted */
		if (!_devices_rom_parsed)
			return;

		/* check that all drivers completed initialization before reporting */
		if (_nvme_driver.constructed() && !_nvme_ns.xml().has_type("controller"))
			return;

		_generate_block_devices(_block_devices);
	}

	Main(Env &env) : _env(env)
	{
		_init_config.enabled(true);
		_block_devices.enabled(true);

		_devices   .sigh(_devices_update_handler);
		_nvme_ns   .sigh(_nvme_ns_update_handler);

		_generate_init_config(_init_config);

		_handle_devices_update();
		_handle_nvme_ns_update();
	}
};


void Driver_manager::Main::_handle_devices_update()
{
	_devices.update();

	if (!_devices.valid())
		return;

	bool has_ahci = false;
	bool has_nvme = false;

	_devices.xml().for_each_sub_node([&] (Xml_node device) {
		device.with_optional_sub_node("pci-config", [&] (Xml_node pci) {

			uint16_t const vendor_id  = (uint16_t)pci.attribute_value("vendor_id",  0U);
			uint16_t const class_code = (uint16_t)(pci.attribute_value("class", 0U) >> 8);

			enum {
				VENDOR_INTEL = 0x8086U,
				CLASS_AHCI   = 0x106U,
				CLASS_NVME   = 0x108U,
			};

			if (vendor_id == VENDOR_INTEL && class_code == CLASS_AHCI)
				has_ahci = true;

			if (class_code == CLASS_NVME)
				has_nvme = true;
		});
	});

	if (!_nvme_driver.constructed() && has_nvme) {
		_nvme_driver.construct();
		_generate_init_config(_init_config);
	}

	_devices_rom_parsed = true;
}


void Driver_manager::Main::_handle_nvme_ns_update()
{
	_nvme_ns.update();
	_generate_block_devices();

	/* update service forwarding rules */
	_generate_init_config(_init_config);
}


void Driver_manager::Main::_generate_init_config(Reporter &init_config) const
{
	Reporter::Xml_generator xml(init_config, [&] () {

		xml.attribute("verbose", false);
		xml.attribute("prio_levels", 2);

		xml.node("report", [&] () {
			xml.attribute("child_ram", true);
			xml.attribute("delay_ms", 2500);
		});

		xml.node("heartbeat", [&] () { xml.attribute("rate_ms", 2500); });

		xml.node("parent-provides", [&] () {
			_gen_parent_service_xml(xml, Rom_session::service_name());
			_gen_parent_service_xml(xml, Io_mem_session::service_name());
			_gen_parent_service_xml(xml, Io_port_session::service_name());
			_gen_parent_service_xml(xml, Cpu_session::service_name());
			_gen_parent_service_xml(xml, Pd_session::service_name());
			_gen_parent_service_xml(xml, Rm_session::service_name());
			_gen_parent_service_xml(xml, Log_session::service_name());
			_gen_parent_service_xml(xml, Timer::Session::service_name());
			_gen_parent_service_xml(xml, Platform::Session::service_name());
			_gen_parent_service_xml(xml, Report::Session::service_name());
			_gen_parent_service_xml(xml, Capture::Session::service_name());
		});

		if (_nvme_driver.constructed())
			_nvme_driver->generate_start_node(xml);

		/* block-service forwarding rules */
		bool const nvme = _nvme_driver.constructed() && _nvme_ns.xml().has_sub_node("namespace");

		if (!nvme) return;

		xml.node("service", [&] () {
			xml.attribute("name", Block::Session::service_name());
				if (nvme)
					_nvme_driver->gen_service_forwarding_policy(xml, "nvme-0");
		});
	});
}


void Driver_manager::Main::_generate_block_devices(Reporter &block_devices) const
{
	Reporter::Xml_generator xml(block_devices, [&] () {

		/* for now just report the first name space */
		if (_nvme_ns.xml().has_sub_node("namespace")) {

			Xml_node nvme_ctrl = _nvme_ns.xml();
			Xml_node nvme_ns   = _nvme_ns.xml().sub_node("namespace");
			xml.node("device", [&] () {

				unsigned long const
					block_count = nvme_ns.attribute_value("block_count", 0UL),
					block_size  = nvme_ns.attribute_value("block_size",  0UL);

				typedef String<40+1> Model;
				Model const model = nvme_ctrl.attribute_value("model", Model());
				typedef String<20+1> Serial;
				Serial const serial = nvme_ctrl.attribute_value("serial", Serial());

				xml.attribute("label",       String<16>("nvme-0"));
				xml.attribute("block_count", block_count);
				xml.attribute("block_size",  block_size);
				xml.attribute("model",       model);
				xml.attribute("serial",      serial);
			});
		}
	});
}


void Component::construct(Genode::Env &env) { static Driver_manager::Main main(env); }
