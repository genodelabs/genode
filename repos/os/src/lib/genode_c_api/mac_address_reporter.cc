/*
 * \brief  C-API Genode MAC address reporter utility
 * \author Christian Helmuth
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/registry.h>
#include <os/reporter.h>
#include <net/mac_address.h>

#include <genode_c_api/mac_address_reporter.h>

using namespace Genode;

typedef String<32> Mac_name;

struct Mac_address : Registry<Mac_address>::Element
{
	Mac_name         name;
	Net::Mac_address addr;

	Mac_address(Registry<Mac_address > &registry,
	            Mac_name         const &name,
	            Net::Mac_address const &addr)
	:
		Registry<Mac_address>::Element(registry, *this),
		name(name), addr(addr)
	{ }

	void report(Xml_generator &report) const
	{
		report.node("nic", [&] () {
			report.attribute("name", name);
			report.attribute("mac_address", String<20>(addr));
		});
	}
};


class Mac_address_registry
{
	private:

		Env       &_env;
		Allocator &_alloc;

		Registry<Mac_address> _registry { };

		Constructible<Expanding_reporter> _reporter { };

		void _report();

	public:

		Mac_address_registry(Env &env, Allocator &alloc)
		: _env(env), _alloc(alloc) { }

		void apply_config(Xml_node const &config);

		void register_address(Mac_name const &name, Net::Mac_address const &addr);
};


void Mac_address_registry::_report()
{
	if (!_reporter.constructed())
		return;

	_reporter->generate([&] (Reporter::Xml_generator &report) {
		_registry.for_each([&] (Mac_address const &e) {
			e.report(report);
		});
	});
}


void Mac_address_registry::register_address(Mac_name const &name,
                                            Net::Mac_address const &addr)
{
	bool found = false;
	_registry.for_each([&] (Mac_address const &e) {
		if (e.addr == addr)
			found = true;
	});
	if (found) return;

	new (_alloc) Mac_address(_registry, name, addr);

	_report();
}


void Mac_address_registry::apply_config(Xml_node const &config)
{
	config.with_optional_sub_node("report", [&] (Xml_node const &xml) {
		if (xml.attribute_value("mac_address", false))
			_reporter.construct(_env, "devices", "devices");
	});

	_report();
}


static Mac_address_registry * _mac_registry = nullptr;

void genode_mac_address_reporter_init(Env &env, Allocator &alloc)
{
	static Mac_address_registry registry { env, alloc };

	_mac_registry = &registry;
}


void genode_mac_address_reporter_config(Xml_node const &config)
{
	if (_mac_registry)
		_mac_registry->apply_config(config);
}


extern "C" void genode_mac_address_register(char const *name, genode_mac_address addr)
{
	if (_mac_registry)
		_mac_registry->register_address(Mac_name(name), addr.addr);
}
