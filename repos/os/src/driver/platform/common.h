/*
 * \brief  Platform driver - compound object for all derivate implementations
 * \author Johannes Schlatow
 * \author Stefan Kalkowski
 * \date   2022-05-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVER__PLATFORM__COMMON_H_
#define _SRC__DRIVER__PLATFORM__COMMON_H_

#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <util/string.h>

#include <device.h>
#include <root.h>

namespace Driver { class Common; };

class Driver::Common : Device_reporter
{
	private:

		Env                     &_env;
		String<64>               _rom_name;
		Attached_rom_dataspace   _devices_rom   { _env, _rom_name.string() };
		Attached_rom_dataspace   _platform_info { _env, "platform_info"    };
		Heap                     _heap          { _env.ram(), _env.rm()    };
		Sliced_heap              _sliced_heap   { _env.ram(), _env.rm()    };

		Device_model _devices { _env, _heap, *this, _kernel_io_mmu() };

		Signal_handler<Common> _dev_handler {
			_env.ep(), *this, &Common::_handle_devices };

		Driver::Root _root;

		Constructible<Expanding_reporter> _cfg_reporter { };
		Constructible<Expanding_reporter> _dev_reporter { };
		Constructible<Expanding_reporter> _iommu_reporter { };

		uint64_t _resume_counter { };

		void _handle_devices();
		bool _kernel_io_mmu();

	public:

		Common(Genode::Env                  &env,
		       Attached_rom_dataspace const &config_rom);

		Heap         & heap()    { return _heap;    }
		Device_model & devices() { return _devices; }

		Node platform_info() { return _platform_info.node(); }

		void announce_service();
		void handle_config(Node const &);

		void report_resume();

		/*********************
		 ** Device_reporter **
		 *********************/

		void update_report() override;
};


void Driver::Common::_handle_devices()
{
	_devices_rom.update();
	_devices.update(_devices_rom.node(), _root);
	update_report();
	_root.update_policy();
}


bool Driver::Common::_kernel_io_mmu()
{
	bool iommu = false;
	_platform_info.node().with_optional_sub_node("kernel", [&] (Node const &node) {
		iommu = node.attribute_value("iommu", false); });

	return iommu;
}


void Driver::Common::update_report()
{
	if (_dev_reporter.constructed())
		_dev_reporter->generate([&] (Generator &g) {
			g.attribute("resumed", _resume_counter);
			_devices.report_devices(g); });
	if (_iommu_reporter.constructed())
		_iommu_reporter->generate([&] (Generator &g) {
			_devices.report_iommus(g); });
}


void Driver::Common::report_resume()
{
	_resume_counter ++;
	update_report();
}


void Driver::Common::handle_config(Node const &config)
{
	config.for_each_sub_node("report", [&] (Node const &node) {
		_dev_reporter.conditional(node.attribute_value("devices", false),
		                          _env, "devices", "devices");
		_cfg_reporter.conditional(node.attribute_value("config", false),
		                          _env, "config", "config");
		_iommu_reporter.conditional(node.attribute_value("iommu", false),
		                            _env, "iommu", "iommu");
	});

	_root.update_policy();

	if (_cfg_reporter.constructed())
		_cfg_reporter->generate([&] (Generator &g) {
			g.node_attributes(config);
			(void)g.append_node_content(config, Generator::Max_depth { 20 }); });
}


void Driver::Common::announce_service()
{
	_env.parent().announce(_env.ep().manage(_root));
}


Driver::Common::Common(Genode::Env                  &env,
                       Attached_rom_dataspace const &config_rom)
:
	_env(env),
	_rom_name(config_rom.node().attribute_value("devices_rom",
	                                            String<64>("devices"))),
	_root(_env, _sliced_heap, config_rom, _devices)
{
	_devices_rom.sigh(_dev_handler);
	_handle_devices();
}

#endif /* _SRC__DRIVER__PLATFORM__COMMON_H_ */
