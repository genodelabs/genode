/*
 * \brief  Cached information of the current runtime configuration
 * \author Norman Feske
 * \date   2019-02-22
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__RUNTIME_CONFIG_H_
#define _MODEL__RUNTIME_CONFIG_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/list_model.h>

/* local includes */
#include <types.h>

namespace Sculpt { class Runtime_config; }

class Sculpt::Runtime_config
{
	private:

		Allocator &_alloc;

		/**
		 * Return target name of route specified by <service> node
		 *
		 * For a route to another child, the target name is the child name.
		 * For a route to the parent, the target name expresses a role of
		 * the parent:
		 *
		 * - 'hardware' provides access to hardware
		 * - 'config' allows the change of the systems configuration
		 * - 'info' reveals system information
		 * - 'GUI' connects to the nitpicker GUI server
		 */
		static Start_name _to_name(Xml_node node)
		{
			Start_name result { };
			node.with_sub_node("child", [&] (Xml_node child) {
				result = child.attribute_value("name", Start_name()); });

			if (result.valid())
				return result;

			node.with_sub_node("parent", [&] (Xml_node parent) {

				typedef String<16> Service;
				Service const service =
					node.attribute_value("name", Service());

				Label const dst_label =
					parent.attribute_value("label", Label());

				bool const ignored_service = (service == "CPU")
				                          || (service == "PD")
				                          || (service == "Report")
				                          || (service == "Timer")
				                          || (service == "LOG");
				if (ignored_service)
					return;

				bool const hardware = (service == "Block")
				                   || (service == "USB")
				                   || (service == "Platform")
				                   || (service == "IO_PORT")
				                   || (service == "IO_MEM")
				                   || (service == "Rtc")
				                   || (service == "IRQ")
				                   || (service == "TRACE");
				if (hardware) {
					result = "hardware";
					return;
				}

				if (service == "ROM") {
					bool const interesting_rom = !dst_label.valid();
					if (interesting_rom) {
						result = "info";
						return;
					}
				}

				if (service == "File_system") {

					if (dst_label == "config") {
						result = "config";
						return;
					}

					if (dst_label == "fonts" || dst_label == "report") {
						result = "info";
						return;
					}
				}

				if (service == "Nitpicker") {
					result = "GUI";
					return;
				}
			});

			return result;
		}

		/**
		 * Return component name targeted by the first route of the start node
		 */
		static Start_name _primary_dependency(Xml_node const start)
		{
			Start_name result { };
			start.with_sub_node("route", [&] (Xml_node route) {
				route.with_sub_node("service", [&] (Xml_node service) {
					result = _to_name(service); }); });

			return result;
		}

	public:

		struct Component : List_model<Component>::Element
		{
			Start_name const name;

			Start_name primary_dependency { };

			struct Dep : List_model<Dep>::Element
			{
				Start_name const to_name;

				Dep(Start_name to_name) : to_name(to_name) { }

				struct Update_policy
				{
					typedef Dep Element;

					Allocator &_alloc;

					Update_policy(Allocator &alloc) : _alloc(alloc) { }

					void destroy_element(Dep &elem) { destroy(_alloc, &elem); }

					Dep &create_element(Xml_node node)
					{
						log("to_name -> ", _to_name(node), " for ", node);
						return *new (_alloc) Dep(_to_name(node));
					}

					void update_element(Dep &, Xml_node) { }

					static bool element_matches_xml_node(Dep const &elem, Xml_node node)
					{
						return _to_name(node) == elem.to_name;
					}

					static bool node_is_element(Xml_node node)
					{
						return _to_name(node).valid();
					}
				};
			};

			/* dependencies on other child components */
			List_model<Dep> deps { };

			template <typename FN>
			void for_each_secondary_dep(FN const &fn) const
			{
				deps.for_each([&] (Dep const &dep) {
					if (dep.to_name != primary_dependency)
						fn(dep.to_name); });
			}

			Component(Start_name const &name) : name(name) { }

			struct Update_policy
			{
				typedef Component Element;

				Allocator &_alloc;

				Update_policy(Allocator &alloc) : _alloc(alloc) { }

				void destroy_element(Component &elem)
				{
					destroy(_alloc, &elem);
				}

				Component &create_element(Xml_node node)
				{
					return *new (_alloc)
						Component(node.attribute_value("name", Start_name()));
				}

				void update_element(Component &elem, Xml_node node)
				{
					log("update component ", elem.name);
					elem.primary_dependency = _primary_dependency(node);

					Dep::Update_policy policy(_alloc);
					node.with_sub_node("route", [&] (Xml_node route) {
						elem.deps.update_from_xml(policy, route); });
				}

				static bool element_matches_xml_node(Component const &elem, Xml_node node)
				{
					return node.attribute_value("name", Start_name()) == elem.name;
				}

				static bool node_is_element(Xml_node node) { return node.has_type("start"); }
			};
		};

	private:

		List_model<Component> _components { };

	public:

		Runtime_config(Allocator &alloc) : _alloc(alloc) { }

		void update_from_xml(Xml_node config)
		{
			Component::Update_policy policy(_alloc);
			_components.update_from_xml(policy, config);
		}

		template <typename FN>
		void for_each_component(FN const &fn) const { _components.for_each(fn); }
};

#endif /* _MODEL__RUNTIME_CONFIG_H_ */
