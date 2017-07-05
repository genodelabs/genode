/*
 * \brief  Tool for turning a subsystem blueprint into an init configuration
 * \author Norman Feske
 * \date   2017-07-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

namespace Depot_deploy {
	using namespace Genode;
	struct Main;
}


struct Depot_deploy::Main
{
	Env &_env;

	Attached_rom_dataspace _config    { _env, "config" };
	Attached_rom_dataspace _blueprint { _env, "blueprint" };

	Reporter _init_config_reporter { _env, "config", "init.config" };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	typedef String<128> Name;
	typedef String<80>  Binary;

	/**
	 * Generate start node of init configuration
	 *
	 * \param pkg     pkg node of the subsystem blueprint
	 * \param common  session routes to be added in addition to the ones
	 *                found in the pkg blueprint
	 */
	static void _gen_start_node(Xml_generator &, Xml_node pkg, Xml_node common);

	void _handle_config()
	{
		_config.update();
		_blueprint.update();

		Xml_node const config    = _config.xml();
		Xml_node const blueprint = _blueprint.xml();

		Reporter::Xml_generator xml(_init_config_reporter, [&] () {

			Xml_node static_config = config.sub_node("static");
			xml.append(static_config.content_base(), static_config.content_size());

			blueprint.for_each_sub_node("pkg", [&] (Xml_node pkg) {

				/*
				 * Check preconditions for generating a '<start>' node.
				 */
				Name const name = pkg.attribute_value("name", Name());

				if (!pkg.has_sub_node("runtime")) {
					warning("<pkg> node for '", name, "' lacks <runtime> node");
					return;
				}

				Xml_node const runtime = pkg.sub_node("runtime");

				if (!runtime.has_sub_node("binary")) {
					warning("<runtime> node for '", name, "' lacks <binary> node");
					return;
				}

				xml.node("start", [&] () {
					_gen_start_node(xml, pkg, config.sub_node("common_routes"));
				});
			});
		});
	}

	Main(Env &env) : _env(env)
	{
		_init_config_reporter.enabled(true);

		_config   .sigh(_config_handler);
		_blueprint.sigh(_config_handler);

		_handle_config();
	}
};


void Depot_deploy::Main::_gen_start_node(Xml_generator &xml, Xml_node pkg, Xml_node common)
{
	typedef String<80> Name;

	Name            const name    = pkg.attribute_value("name", Name());
	Xml_node        const runtime = pkg.sub_node("runtime");
	size_t          const caps    = runtime.attribute_value("caps", 0UL);
	Number_of_bytes const ram     = runtime.attribute_value("ram", Number_of_bytes());
	Binary          const binary  = runtime.sub_node("binary").attribute_value("name", Binary());

	xml.attribute("name", name);
	xml.attribute("caps", caps);

	xml.node("binary", [&] () { xml.attribute("name", binary); });

	xml.node("resource", [&] () {
		xml.attribute("name", "RAM");
		xml.attribute("quantum", String<32>(ram));
	});

	/*
	 * Insert inline '<config>' node if provided by the blueprint.
	 */
	if (runtime.has_sub_node("config")) {
		Xml_node config = runtime.sub_node("config");
		xml.node("config", [&] () {
			xml.append(config.content_base(), config.content_size());
		});
	};

	xml.node("route", [&] () {

		/*
		 * Add ROM routing rule with the label rewritten to
		 * the path within the depot.
		 */
		pkg.for_each_sub_node("rom", [&] (Xml_node rom) {

			if (!rom.has_attribute("path"))
				return;

			typedef String<160> Path;
			typedef Name        Label;
			Path  const path  = rom.attribute_value("path",  Path());
			Label const label = rom.attribute_value("label", Label());

			xml.node("service", [&] () {
				xml.attribute("name", "ROM");
				xml.attribute("label", label);
				xml.node("parent", [&] () {
					xml.attribute("label", path);
				});
			});
		});

		/*
		 * Add common routes as defined in our config.
		 */
		xml.append(common.content_base(), common.content_size());
	});
}


void Component::construct(Genode::Env &env) { static Depot_deploy::Main main(env); }

