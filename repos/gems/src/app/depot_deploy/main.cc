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
#include <util/retry.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>

/* local includes */
#include "child.h"

namespace Depot_deploy {
	struct Children;
	struct Main;
}


class Depot_deploy::Children
{
	private:

		Allocator &_alloc;

		List_model<Child> _children { };

		struct Model_update_policy : List_model<Child>::Update_policy
		{
			Allocator &_alloc;

			Model_update_policy(Allocator &alloc) : _alloc(alloc) { }

			void destroy_element(Child &c) { destroy(_alloc, &c); }

			Child &create_element(Xml_node node)
			{
				return *new (_alloc) Child(_alloc, node);
			}

			void update_element(Child &c, Xml_node node) { c.apply_config(node); }

			static bool element_matches_xml_node(Child const &child, Xml_node node)
			{
				return node.attribute_value("name", Child::Name()) == child.name();
			}

			static bool node_is_element(Xml_node node) { return node.has_type("start"); }

		} _model_update_policy { _alloc };

	public:

		Children(Allocator &alloc) : _alloc(alloc) { }

		void apply_config(Xml_node config)
		{
			_children.update_from_xml(_model_update_policy, config);
		}

		void apply_blueprint(Xml_node blueprint)
		{
			blueprint.for_each_sub_node("pkg", [&] (Xml_node pkg) {
				_children.for_each([&] (Child &child) {
					child.apply_blueprint(pkg); }); });
		}

		void gen_start_nodes(Xml_generator &xml, Xml_node common)
		{
			_children.for_each([&] (Child const &child) {
				child.gen_start_node(xml, common); });
		}

		void gen_queries(Xml_generator &xml)
		{
			_children.for_each([&] (Child const &child) {
				child.gen_query(xml); });
		}
};


struct Depot_deploy::Main
{
	Env &_env;

	Attached_rom_dataspace _config    { _env, "config" };
	Attached_rom_dataspace _blueprint { _env, "blueprint" };

	Expanding_reporter _query_reporter       { _env, "query" , "query"};
	Expanding_reporter _init_config_reporter { _env, "config", "init.config"};

	size_t _query_buffer_size       = 4096;
	size_t _init_config_buffer_size = 4096;

	Heap _heap { _env.ram(), _env.rm() };

	Children _children { _heap };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	typedef String<128> Name;

	void _handle_config()
	{
		_config.update();
		_blueprint.update();

		Xml_node const config = _config.xml();

		_children.apply_config(config);
		_children.apply_blueprint(_blueprint.xml());

		/* determine CPU architecture of deployment */
		typedef String<16> Arch;
		Arch const arch = config.attribute_value("arch", Arch());
		if (!arch.valid())
			warning("config lacks 'arch' attribute");

		/* generate init config containing all configured start nodes */
		_init_config_reporter.generate([&] (Xml_generator &xml) {
			Xml_node static_config = config.sub_node("static");
			xml.append(static_config.content_base(), static_config.content_size());
			_children.gen_start_nodes(xml, config.sub_node("common_routes"));
		});

		/* update query for blueprints of all unconfigured start nodes */
		if (arch.valid()) {
			_query_reporter.generate([&] (Xml_generator &xml) {
				xml.attribute("arch", arch);
				_children.gen_queries(xml);
			});
		}
	}

	Main(Env &env) : _env(env)
	{
		_config   .sigh(_config_handler);
		_blueprint.sigh(_config_handler);

		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Depot_deploy::Main main(env); }

