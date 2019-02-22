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
		 * Return component name targeted by the first route of the start node
		 */
		static Start_name _primary_dependency(Xml_node const start)
		{
			if (!start.has_sub_node("route"))
				return Start_name();

			Xml_node const route = start.sub_node("route");

			if (!route.has_sub_node("service"))
				return Start_name();

			Xml_node const service = route.sub_node("service");

			if (service.has_sub_node("child")) {
				Xml_node const child = service.sub_node("child");
				return child.attribute_value("name", Start_name());
			}

			return Start_name();
		}

	public:

		struct Component : List_model<Component>::Element
		{
			Start_name const name;

			Start_name primary_dependency { };

			struct Child_dep : List_model<Child_dep>::Element
			{
				Start_name const to_name;

				Child_dep(Start_name to_name) : to_name(to_name) { }

				struct Update_policy
				{
					typedef Child_dep Element;

					Allocator &_alloc;

					Update_policy(Allocator &alloc) : _alloc(alloc) { }

					void destroy_element(Child_dep &elem) { destroy(_alloc, &elem); }

					static Start_name _to_name(Xml_node node)
					{
						Start_name result { };
						node.with_sub_node("child", [&] (Xml_node child) {
							result = child.attribute_value("name", Start_name()); });

						return result;
					}

					Child_dep &create_element(Xml_node node)
					{
						return *new (_alloc) Child_dep(_to_name(node));
					}

					void update_element(Child_dep &, Xml_node) { }

					static bool element_matches_xml_node(Child_dep const &elem, Xml_node node)
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
			List_model<Child_dep> child_deps { };

			template <typename FN>
			void for_each_secondary_dep(FN const &fn) const
			{
				child_deps.for_each([&] (Child_dep const &dep) {
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
					elem.primary_dependency = _primary_dependency(node);

					Child_dep::Update_policy policy(_alloc);
					node.with_sub_node("route", [&] (Xml_node route) {
						elem.child_deps.update_from_xml(policy, route); });
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
