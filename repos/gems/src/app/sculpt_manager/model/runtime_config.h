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
#include <util/dictionary.h>
#include <util/progress.h>
#include <base/registry.h>
#include <dialog/types.h>
#include <depot/archive.h>

/* local includes */
#include <types.h>
#include <model/service.h>

namespace Sculpt { class Runtime_config; }


class Sculpt::Runtime_config
{
	private:

		using Progress = Genode::Progress;

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
		static Start_name _to_name(Node const &node)
		{
			Start_name result { };
			node.with_optional_sub_node("child", [&] (Node const &child) {
				result = child.attribute_value("name", Start_name()); });

			if (result.valid())
				return result;

			node.with_optional_sub_node("parent", [&] (Node const &) {

				Service::Type_name const service =
					node.attribute_value("name", Service::Type_name());

				bool const ignored_service = (service == "CPU")
				                          || (service == "PD")
				                          || (service == "Timer")
				                          || (service == "LOG")
				                          || (service == "TRACE");
				if (ignored_service)
					return;

				bool const hardware = (service == "IO_PORT")
				                   || (service == "IO_MEM")
				                   || (service == "IRQ");
				if (hardware) {
					result = "hardware";
					return;
				}
			});

			return result;
		}

		/**
		 * Return component name targeted by the first route of the start node
		 */
		static Start_name _primary_dependency(Node const &start)
		{
			Start_name result { };
			start.with_optional_sub_node("route", [&] (Node const &route) {
				route.with_optional_sub_node("service", [&] (Node const &service) {
					result = _to_name(service); }); });

			return result;
		}

		struct Child_service : Service, List_model<Child_service>::Element
		{
			static Service::Type type_from_node(Node const &service)
			{
				Service::Type_name const type = service.type();
				for (unsigned i = 0; i < (unsigned)Type::UNDEFINED; i++) {
					Type const t = (Type)i;
					if (type == Service::node_type(t))
						return t;
				}
				return Type::UNDEFINED;
			}

			static Service::Name name_from_node(Node const &service)
			{
				return service.attribute_value("name", Service::Name());
			}

			Child_service(Start_name server, Node const &service)
			:
				Service(server, type_from_node(service), name_from_node(service))
			{ }

			static bool type_matches(Node const &node)
			{
				return type_from_node(node) != Service::Type::UNDEFINED;
			}

			bool matches(Node const &node) const
			{
				return type_from_node(node) == type
				    && name_from_node(node) == name;
			}
		};

		/*
		 * Data structure to associate dialog widget IDs with component names.
		 */
		struct Graph_id;

		using Graph_ids = Dictionary<Graph_id, Start_name>;

		struct Graph_id : Graph_ids::Element, Dialog::Id
		{
			Graph_id(Graph_ids &dict, Start_name const &name, Dialog::Id const &id)
			:
				Graph_ids::Element(dict, name), Dialog::Id(id)
			{ }
		};

		Graph_ids _graph_ids { };

		unsigned _graph_id_count = 0;

		unsigned _num_stalled = 0;

	public:

		struct Component : List_model<Component>::Element
		{
			Start_name const name;

			Graph_id const graph_id;

			Start_name primary_dependency { };

			struct Dep : List_model<Dep>::Element
			{
				Start_name const to_name;

				Dep(Start_name to_name) : to_name(to_name) { }

				bool matches(Node const &node) const
				{
					return _to_name(node) == to_name;
				}

				static bool type_matches(Node const &node)
				{
					return _to_name(node).valid();
				}
			};

			/* dependencies on other child components */
			List_model<Dep> deps { };

			void for_each_secondary_dep(auto const &fn) const
			{
				deps.for_each([&] (Dep const &dep) {
					if (dep.to_name != primary_dependency)
						fn(dep.to_name); });
			}

			List_model<Child_service> _child_services { };

			Constructible<Buffered_node> _stalled { };

			Component(Start_name const &name, Graph_ids &graph_ids, Dialog::Id const &id)
			:
				name(name), graph_id(graph_ids, name, id)
			{ }

			void for_each_service(auto const &fn) const
			{
				_child_services.for_each(fn);
			}

			Progress update_route_from_node(Allocator &alloc, Node const &route)
			{
				Progress result = STALLED;
				deps.update_from_node(route,

					/* create */
					[&] (Node const &node) -> Dep & {
						result = PROGRESSED;
						return *new (alloc) Dep(_to_name(node)); },

					/* destroy */
					[&] (Dep &e) { result = PROGRESSED; destroy(alloc, &e); },

					/* update */
					[&] (Dep &, Node const &) { }
				);
				return result;
			}

			Progress update_provides_from_node(Allocator &alloc, Node const &provides)
			{
				Progress result = STALLED;
				_child_services.update_from_node(provides,

					/* create */
					[&] (Node const &node) -> Child_service & {
						result = PROGRESSED;
						return *new (alloc) Child_service(name, node); },

					/* destroy */
					[&] (Child_service &e) { result = PROGRESSED; destroy(alloc, &e); },

					/* update */
					[&] (Child_service &, Node const &) { }
				);
				return result;
			}

			static bool stalled(Node const &n) { return n.has_type("stalled"); }

			Progress update_from_node(Allocator &alloc, Node const &node)
			{
				Progress result = STALLED;

				auto const orig_primary_dependency = primary_dependency;
				primary_dependency = _primary_dependency(node);
				if (orig_primary_dependency != primary_dependency)
					result = PROGRESSED;

				node.with_optional_sub_node("route", [&] (Node const &route) {
					if (update_route_from_node(alloc, route).progressed)
						result = PROGRESSED; });

				node.with_optional_sub_node("deploy", [&] (Node const &deploy) {
					deploy.with_optional_sub_node("provides", [&] (Node const &provides) {
						if (update_provides_from_node(alloc, provides).progressed)
							result = PROGRESSED; });
				});

				bool const stalled_differs =
					stalled(node) ? !_stalled.constructed() || _stalled->differs_from(node)
					              :  _stalled.constructed();
				if (stalled_differs) {
					_stalled.destruct();
					if (stalled(node))
						_stalled.construct(alloc, node);
					result = PROGRESSED;
				}
				return result;
			}

			bool matches(Node const &node) const
			{
				return node.attribute_value("name", Start_name()) == name;
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("start") || stalled(node);
			}
		};

	private:

		List_model<Component> _components { };

		struct Parent_services
		{
			using Parent_service = Registered_no_delete<Service>;
			using Type = Service::Type;

			Registry<Parent_service> _r { };

			Parent_service const
				_pf_info   { _r, Type::ROM,      "platform information", "platform_info" },
				_bld_info  { _r, Type::ROM,      "build information",    "build_info" },
				_rm        { _r, Type::RM,       "custom virtual memory objects" },
				_io_mem    { _r, Type::IO_MEM,   "raw hardware access" },
				_io_port   { _r, Type::IO_PORT,  "raw hardware access" },
				_irq       { _r, Type::IRQ,      "raw hardware access" },
				_trace_all { _r, Type::TRACE,    "system",      "global" },
				_trace_rt  { _r, Type::TRACE,    "deployment",  "runtime" },
				_trace     { _r, Type::TRACE,    "component" },
				_vm        { _r, Type::VM,       "virtualization hardware" },
				_pd        { _r, Type::PD,       "system PD service" },
				_monitor   { _r, Type::TERMINAL, "debug monitor" };

			void for_each(auto const &fn) const { _r.for_each(fn); }

		} _parent_services { };

		Service const _used_fs_service { "default_fs_rw",
		                                 Service::Type::FS,
		                                 { }, "used file system" };

	public:

		Runtime_config(Allocator &alloc) : _alloc(alloc) { }

		Progress update_from_node(Node const &config)
		{
			Progress result = STALLED;
			_num_stalled = 0;
			_components.update_from_node(config,

				/* create */
				[&] (Node const &node) -> Component & {
					result = PROGRESSED;
					return *new (_alloc)
						Component(node.attribute_value("name", Start_name()),
						          _graph_ids,
						          Dialog::Id { _graph_id_count++ });
				},

				/* destroy */
				[&] (Component &e) {

					/* flush list models */
					e.update_route_from_node   (_alloc, Node());
					e.update_provides_from_node(_alloc, Node());

					destroy(_alloc, &e);
					result = PROGRESSED;
				},

				/* update */
				[&] (Component &e, Node const &node) {
					if (node.has_type("stalled")) _num_stalled++;
					if (e.update_from_node(_alloc, node).progressed)
						result = PROGRESSED;
				}
			);
			return result;
		}

		bool present_in_runtime(Start_name const &name) const
		{
			bool result = false;
			_components.for_each([&] (Component const &component) {
				if (component.name == name)
					result = true; });
			return result;
		}

		void with_start_name(Dialog::Id const &id, auto const &fn) const
		{
			_components.for_each([&] (Component const &component) {
				if (component.graph_id == id)
					fn(component.name); });
		}

		void with_graph_id(Start_name const &name, auto const &fn) const
		{
			_graph_ids.with_element(name,
				[&] (Graph_id const &id) { fn(id); },
				[&] { });
		}

		void for_each_component(auto const &fn) const { _components.for_each(fn); }

		void for_each_stalled(auto const &fn) const
		{
			_components.for_each([&] (Component const &component) {
				if (component._stalled.constructed()) {
					Node const &stalled = *component._stalled;
					fn(stalled); } });
		}

		bool any_stalled() const { return _num_stalled > 0; }

		void for_each_missing_pkg(auto const &fn) const
		{
			for_each_stalled([&] (Node const &stalled) {
				stalled.with_optional_sub_node("deploy", [&] (Node const &deploy) {
					deploy.with_optional_sub_node("pkg_missing", [&] (Node const &) {
						fn(deploy.attribute_value("pkg", Depot::Archive::Path())); }); }); });
		}

		/**
		 * Call 'fn' with the name of each dependency of component 'name'
		 */
		void for_each_dependency(Start_name const &name, auto const &fn) const
		{
			_components.for_each([&] (Component const &component) {
				if (component.name == name) {
					component.deps.for_each([&] (Component::Dep const &dep) {
						fn(dep.to_name); }); } });
		}

		void for_each_service(auto const &fn) const
		{
			_parent_services.for_each(fn);

			fn(_used_fs_service);

			_components.for_each([&] (Component const &component) {
				component.for_each_service(fn); });
		}

		unsigned num_service_options(Service::Type const type) const
		{
			unsigned count = 0;
			for_each_service([&] (Service const &service) {
				if (service.type == type)
					count++; });
			return count;
		}
};

#endif /* _MODEL__RUNTIME_CONFIG_H_ */
