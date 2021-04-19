/*
 * \brief  spi root component
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-04-28
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPI_DRIVER__COMPONENT_H_
#define _INCLUDE__SPI_DRIVER__COMPONENT_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/heap.h>
#include <root/component.h>
#include <spi_session/spi_session.h>

/* Local includes */
#include "spi_driver.h"

namespace Spi {

	using namespace Genode;
	class Session_resources;
	class Session_component;
	class Root;

}

class Spi::Session_resources
{
protected:

	Ram_quota_guard           _ram_guard;
	Cap_quota_guard           _cap_guard;
	Constrained_ram_allocator _ram_allocator;
	Attached_ram_dataspace    _io_buffer;

	Session_resources(Genode::Ram_allocator &ram, Genode::Region_map &region_map, Genode::Ram_quota ram_quota,
	                  Genode::Cap_quota cap_quota, size_t io_buffer_size)
	:
		_ram_guard     { ram_quota },
		_cap_guard     { cap_quota },
		_ram_allocator { ram, _ram_guard, _cap_guard },
		_io_buffer     { _ram_allocator, region_map, io_buffer_size }
	{ }
};

class Spi::Session_component : private Spi::Session_resources,
                               public Genode::Rpc_object<Spi::Session, Spi::Session_component>
{
private:

	Driver  &_driver;
	size_t   _slave_select;
	Settings _settings;

public:

	Session_component(Env         &env,
	                  Ram_quota    ram_quota,
	                  Cap_quota    cap_quota,
	                  Spi::Driver &driver,
	                  size_t       io_buffer_size,
	                  size_t       slave_select,
	                  Settings    &session_settings)
	:
		Spi::Session_resources { env.pd(), env.rm(), ram_quota, cap_quota, io_buffer_size },
		_driver                { driver },
		_slave_select          { slave_select },
		_settings              { session_settings }
	{ }

	size_t spi_transfer(size_t buffer_size) {
		Driver::Transaction transaction {
			.settings     = _settings,
			.slave_select = _slave_select,
			.buffer       = _io_buffer.local_addr<uint8_t>(),
			.size         = buffer_size,
		};
		return _driver.transfer(transaction);
	}

	Dataspace_capability io_buffer_dataspace() { return _io_buffer.cap(); }

	void     settings(Settings settings) override { _settings = settings; }
	Settings settings() override { return _settings; }

	/* Client interface unused by the driver */
	size_t transfer(char*, size_t) override { return 0; }
};


class Spi::Root : public Genode::Root_component<Spi::Session_component>
{
private:

	Env           &_env;
	Driver        &_driver;
	Xml_node const _config;


	size_t _parse_policy(char const *args, Settings &settings) const
	{
		char device_name[64];
		Genode::Arg_string::find_arg(args, "label").string(device_name, 64, "");

		int slave_select = -1;
		_config.for_each_sub_node([&slave_select, &settings,  &device_name] (Genode::Xml_node const &node) {
			if (node.type() == "policy") {
				Genode::String<64> label;
				label = node.attribute_value("label_prefix", label);
				if (label == device_name) {
					slave_select = static_cast<int>(_parse_policy_xml_node(node, settings));
				}
			}
		});

		if (slave_select == -1) {
			Genode::warning("Session with label ",
			                Genode::Cstring { device_name },
			                " could not be created, no such policy.");
			throw Genode::Service_denied();
		}

		return slave_select;
	}

	static unsigned long _parse_policy_xml_node(Genode::Xml_node const &node, Settings &settings) {
		settings.mode = node.attribute_value("mode", settings.mode);
		settings.clock_idle_state = node.attribute_value("clock_idle_state", settings.clock_idle_state);
		settings.data_lines_idle_state = node.attribute_value("data_lines_active_state", settings.data_lines_idle_state);
		settings.ss_line_active_state = node.attribute_value("ss_line_active_state", settings.ss_line_active_state);
		return node.attribute_value("slave_select", static_cast<unsigned long>(0));
	}

protected:

	Session_component* _create_session(const char *args) override
	{
		size_t const ram_quota = Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		size_t const cap_quota = Genode::Arg_string::find_arg(args, "cap_quota").ulong_value(0);
		size_t const io_buffer_size = Genode::Arg_string::find_arg(args, "io_buffer_size").ulong_value(0);

		if (ram_quota < sizeof(Session_component) + io_buffer_size) {
			Genode::error("insufficient donated ram_quota "
			              "(", ram_quota, " bytes), "
			              "require ", sizeof(Session_component) + io_buffer_size , " bytes");
			throw Genode::Insufficient_ram_quota();
		}

		Settings     session_settings = { };
		size_t const slave_select     = _parse_policy(args, session_settings);

		return new (md_alloc()) Spi::Session_component(_env,
		                                               Genode::Ram_quota { ram_quota },
		                                               Genode::Cap_quota { cap_quota },
		                                               _driver,
		                                               io_buffer_size,
		                                               slave_select,
		                                               session_settings);
	}

public:

	Root(Genode::Env            &env,
	     Genode::Heap           &heap,
	     Spi::Driver            &diver,
	     Genode::Xml_node const &config)
	:
		Genode::Root_component<Spi::Session_component> { env.ep(), heap },
		_env                                           { env },
		_driver                                        { diver },
		_config                                        { config }
	{ }
};

#endif // _INCLUDE__SPI_DRIVER__COMPONENT_H_
