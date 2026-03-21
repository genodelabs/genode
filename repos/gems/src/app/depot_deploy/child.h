/*
 * \brief  Child representation
 * \author Norman Feske
 * \date   2018-01-23
 */

/*
 * Copyright (C) 2018-2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* Genode includes */
#include <util/list_model.h>
#include <util/dictionary.h>
#include <util/progress.h>
#include <depot/archive.h>

/* local includes */
#include "resource.h"

namespace Depot_deploy {

	using namespace Depot;

	static constexpr Generator::Max_depth MAX_NODE_DEPTH = { 20 };

	struct Child;
	struct Alias;

	using Child_name       = String<100>;
	using Option_name      = String<100>;
	using Depot_rom_server = String<32>;

	using Child_dict  = Dictionary<Child, Child_name>;
	using Alias_dict  = Dictionary<Alias, Child_name>;

	static inline void with_server_name(Child_name const &name_or_alias,
	                                    Alias_dict const &, auto const &);

	struct Duplicate_checked
	{
		bool const duplicate;

		Duplicate_checked(Child_dict &dict, Child_name const &name)
		:
			duplicate(dict.exists(name))
		{
			if (duplicate)
				warning("omitting child with duplicated name '", name, "'");
		}
	};

	struct Prio_levels
	{
		unsigned value;

		int min_priority() const
		{
			return (value > 0) ? -(int)(value - 1) : 0;
		}
	};

	struct Global
	{
		Resource::Types  const &resource_types;
		Child_dict       const &child_dict;
		Alias_dict       const &alias_dict;
		Node             const &common;
		Prio_levels             prio_levels;
		Affinity::Space  const affinity_space;
		Depot_rom_server const &default_depot_rom;
	};
}


class Depot_deploy::Child : public Duplicate_checked,
                            public List_model<Child>::Element,
                            public Child_dict::Element
{
	public:

		using Name        = Child_name;
		using Binary_name = String<80>;
		using Config_name = String<80>;

		static Name node_name(Node const &node)
		{
			return node.attribute_value("name", Name());
		}

	private:

		Constructible<Buffered_node> _child_node { };  /* from config */
		Constructible<Buffered_node> _pkg_node   { };  /* from blueprint */

		static auto _with_node(auto &constructible, auto const &fn,
		                       auto const &missing_fn) -> decltype(missing_fn())
		{
			if (!constructible.constructed())
				return missing_fn();

			Node const &node = *constructible;
			return fn(node);
		}

		static auto _identical(auto &constructible_node, Node const &node)
		{
			return _with_node(constructible_node,
				[&] (Node const &orig) { return !node.differs_from(orig); },
				[&]                    { return false; });
		}

		Archive::Path _pkg_path(Node const &child) const
		{
			return child.attribute_value("pkg", Archive::Path());
		}

		/*
		 * The pkg-archive path of the current blueprint query, which may
		 * deviate from pkg path given in the config, once the config is
		 * updated.
		 */
		Archive::Path _blueprint_pkg_path { };

		/*
		 * Quota definitions obtained from the blueprint
		 */
		Num_bytes     _pkg_ram_quota { 0 };
		unsigned long _pkg_cap_quota { 0 };

		Binary_name _binary_name { };
		Config_name _config_name { };

		Depot_rom_server _custom_depot_rom { };

		bool _enabled = true;

		/*
		 * Set if the depot query for the child's blueprint failed.
		 */
		enum class Pkg {
			UNKNOWN,      /* child created but not yet updated */
			NONE,         /* deployed from ROM with no pkg needed */
			DISCOVER,     /* blueprint not yet known */
			MISSING,      /* pkg not installed */
			CORRUPT,      /* blueprint is inconsistent */
			COMPLETE      /* pkg ready to use */
		};

		Pkg _pkg = Pkg::UNKNOWN;

		bool _pkg_satisfied() const { return _pkg == Pkg::NONE
		                                  || _pkg == Pkg::COMPLETE; }

		/*
		 * State of the dependency of the child from other children
		 */
		enum class Deps {
			UNKNOWN,  /* not checked */
			MISSING,  /* any server is missing from the deployment */
			STALLED,  /* any server is yet READY */
			READY     /* all preconditions are satisfied */
		};

		Deps _deps = Deps::UNKNOWN;

		bool _deps_satisfied() const { return _deps == Deps::READY; }

		bool _discovered(Node const &child) const
		{
			if (_pkg == Pkg::NONE)
				return true;

			/* pkg attribute changed */
			return _pkg_node.constructed()
			   && (_pkg_path(child) == _blueprint_pkg_path);
		}

		inline void _gen_routes(Generator &, Global const &, Node const &,
		                        Depot_rom_server const &) const;

		/*
		 * node is pkg runtime or deploy child (Pkg::NONE)
		 */
		static void _gen_provides(Generator &g, Resource::Types const &types, Node const &node)
		{
			node.with_optional_sub_node("provides", [&] (Node const &provides) {
				g.node("provides", [&] {
					provides.for_each_sub_node([&] (Node const &service) {
						Resource const res = Resource::from_node_type(types, service.type());
						if (res.type != Resource::UNDEFINED)
							g.node("service", [&] {
								g.attribute("name", res.service_name()); }); }); }); });
		}

		static void _gen_copy_of_sub_node(Generator &g, Node const &from_node,
		                                  Node::Type const &sub_node_type)
		{
			from_node.with_optional_sub_node(sub_node_type.string(),
				[&] (Node const &sub_node) {
					if (!g.append_node(sub_node, MAX_NODE_DEPTH))
						warning("sub node exceeds max depth: ", from_node); });
		}

		inline void _gen_start_node(Generator &, Node const &,
		                            Option_name const &, Global const &) const;

		void _for_each_dep_status(Child_dict const &child_dict,
		                          Alias_dict const &alias_dict, auto const &fn) const
		{
			auto for_each_dep = [&] (auto const &fn)
			{
				_with_node(_child_node, [&] (Node const &child) {
					child.with_optional_sub_node("connect", [&] (Node const &connect) {
						connect.for_each_sub_node([&] (Node const &connection) {
							connection.with_optional_sub_node("child", [&] (Node const &server) {
								fn(server.attribute_value("name", Name()), connection); }); }); });
					/* deprecated */
					child.with_optional_sub_node("route", [&] (Node const &connect) {
						connect.for_each_sub_node([&] (Node const &service) {
							service.with_optional_sub_node("child", [&] (Node const &server) {
								fn(server.attribute_value("name", Name()), service); }); }); });
				}, [] { });
			};

			for_each_dep([&] (Name const &child_or_alias, Node const &conn) {
				with_server_name(child_or_alias, alias_dict, [&] (Name const &name) {
					child_dict.with_element(name,
						[&] (Child const &server) {
							fn(server.runnable() ? Deps::READY : Deps::STALLED, name, conn); },
						[&] {
							fn(Deps::MISSING, name, conn); }); }); });
		}

		void _with_blueprint(auto const &fn) const
		{
			if (_pkg_node.constructed()) {
				Node const &node = *_pkg_node;
				fn(node);
			}
		}

		void _for_each_missing_rom(auto const &fn) const
		{
			_with_blueprint([&] (Node const &blueprint) {
				blueprint.for_each_sub_node("missing_rom", [&] (Node const &missing_rom) {
					Name const name = missing_rom.attribute_value("name", Name());

					/* ld.lib.so is special because it is provided by the base system */
					if (name == "ld.lib.so")
						return;

					fn(name);
				});
			});
		}

		static bool _config_overridden(Node const &child)
		{
			return !child.attribute_value("config", true);
		}

	public:

		Child(Child_dict &dict, Name const &name)
		:
			Duplicate_checked(dict, name), Child_dict::Element(dict, name)
		{ }

		Progress apply_config(Allocator &alloc, Node const &child)
		{
			if (_identical(_child_node, child))
				return STALLED;

			Archive::Path const orig_pkg_path = _blueprint_pkg_path;

			/* import new child node */
			_child_node.construct(alloc, child);

			_blueprint_pkg_path = _pkg_path(child);

			/* invalidate blueprint if 'pkg' path changed */
			if (orig_pkg_path != _blueprint_pkg_path) {
				_pkg_node.destruct();

				/* reset error state, attempt to obtain the blueprint again */
				_pkg = Pkg::DISCOVER;
			}

			if (child.has_attribute("binary")) {
				_pkg = Pkg::NONE;
				_pkg_node.destruct();
				_binary_name = child.attribute_value("binary", Binary_name());
				_config_name = child.attribute_value("config", Config_name());
			}

			_custom_depot_rom = child.attribute_value("depot_rom", Depot_rom_server());

			/*
			 * Accept any other value than "yes" for "no". This allows for
			 * the encoding of contextual information as attribute values,
			 * e.g., 'enabled: discover' to express that the start of a driver
			 * depends on hardware discovered at runtime.
			 */
			_enabled = (child.attribute_value("enabled", String<16>("yes")) == "yes");

			return PROGRESSED;
		}

		/*
		 * \return true if bluerprint had an effect on the child
		 */
		Progress apply_blueprint(Allocator &alloc, Node const &pkg)
		{
			if (_pkg != Pkg::DISCOVER)
				return STALLED;

			/* blueprint is unrelated */
			if (pkg.attribute_value("path", Archive::Path()) != _blueprint_pkg_path)
				return STALLED;

			if (pkg.type() == "missing") {
				_pkg = Pkg::MISSING;
				return PROGRESSED;
			}

			if (pkg.type() != "pkg")
				return STALLED;

			/* keep copy of the blueprint info */
			_pkg_node.construct(alloc, pkg);

			_pkg = Pkg::COMPLETE;

			/* check for the completeness of all ROM ingredients */
			_for_each_missing_rom([&] (Name const &) { _pkg = Pkg::CORRUPT; });

			pkg.with_sub_node("runtime",
				[&] (Node const &runtime) {

					_pkg_ram_quota = runtime.attribute_value("ram", Num_bytes());
					_pkg_cap_quota = runtime.attribute_value("caps", 0UL);

					_binary_name = runtime.attribute_value("binary", Binary_name());
					_config_name = runtime.attribute_value("config", Config_name());
				},
				[&] {
					_pkg = Pkg::CORRUPT;
				});

			return PROGRESSED;
		}

		void rediscover_blueprint()
		{
			if (_pkg != Pkg::NONE)
				_pkg = Pkg::DISCOVER;
		}

		void gen_query(Generator &g) const
		{
			auto blueprint_needed = [&]
			{
				if (_pkg == Pkg::NONE)     return false;
				if (_pkg == Pkg::DISCOVER) return true;

				return _with_node(_child_node,
					[&] (Node const &child) { return !_discovered(child); },
					[&]                     { return false; });
			};

			if (blueprint_needed())
				g.node("blueprint", [&] {
					g.attribute("pkg", _blueprint_pkg_path); });
		}

		void gen_start_node(Generator &g, Option_name const &option,
		                    Global const &global) const
		{
			if (_enabled)
				_with_node(_child_node, [&] (Node const &child) {
					_gen_start_node(g, child, option, global); }, [&] { });
		}

		inline void gen_monitor_policy_node(Generator &) const;

		bool runnable() const { return _pkg_satisfied() && _deps_satisfied(); };

		/**
		 * List_model::Element
		 */
		bool matches(Node const &node) const { return node_name(node) == name; }

		/**
		 * List_model::Element
		 */
		static bool type_matches(Node const &node)
		{
			if (node.has_type("start")) {
				static bool warned_once;
				if (!warned_once) {
					warning("start node should be a child node: ", node);
					warned_once = true;
				}
			}
			return node.has_type("child") || node.has_type("start");
		}

		void reset_deps() { _deps = Deps::UNKNOWN; }

		Progress update_deps(Child_dict const &child_dict, Alias_dict const &alias_dict)
		{
			if (_deps_satisfied())
				return STALLED;

			Deps const orig_deps = _deps;
			_deps = Deps::READY;
			_for_each_dep_status(child_dict, alias_dict,
				[&] (Deps const &deps, Name const &, Node const &) {
					/* missing is stronger than stalled */
					if (_deps == Deps::MISSING) return;
					if (deps == Deps::MISSING) _deps = Deps::MISSING;
					if (deps == Deps::STALLED) _deps = Deps::STALLED;
				});
			return { .progressed = orig_deps != _deps };
		}

		static Progress update_from_node(List_model<Child> &children,
		                                 Child_dict &dict,
		                                 Allocator &alloc, Node const &node)
		{
			Progress result { };
			children.update_from_node(node,

				/* create */
				[&] (Node const &node) -> Child & {
					result = PROGRESSED;
					return *new (alloc)
						Child(dict, Child::node_name(node)); },

				/* destroy */
				[&] (Child &child) {
					result = PROGRESSED;
					destroy(alloc, &child); },

				/* update */
				[&] (Child &child, Node const &node) {
					if (child.apply_config(alloc, node).progressed)
						result = PROGRESSED; }
			);
			return result;
		}
};


void Depot_deploy::Child::_gen_start_node(Generator         &g,
                                          Node        const &child,
                                          Option_name const &option,
                                          Global      const &global) const
{
	bool const stalled = !runnable();

	g.node(stalled ? "stalled" : "start", [&] {

		g.attribute("name", name);

		struct Attr
		{
			using Version  = String<64>;
			using Location = Affinity::Location;

			unsigned long caps;
			Num_bytes     ram;
			Version       version;
			long          priority;
			bool          ld;
			bool          system;
			Location      location;
		};

		Attr attr { };
		attr.caps     = child.attribute_value("caps",     _pkg_cap_quota);
		attr.ram      = child.attribute_value("ram",      _pkg_ram_quota);
		attr.version  = child.attribute_value("version",  attr.version);
		attr.priority = child.attribute_value("priority", global.prio_levels.min_priority());
		attr.ld       = child.attribute_value("ld",       true);
		attr.system   = child.attribute_value("managing_system", false);

		child.with_optional_sub_node("affinity", [&] (Node const &node) {
			attr.location = Attr::Location::from_node(global.affinity_space, node); });

		g.attribute("ram",  attr.ram);
		g.attribute("caps", attr.caps);
		if (attr.version.length() > 1) g.attribute("version",  attr.version);
		if (attr.priority)             g.attribute("priority", attr.priority);
		if (attr.system)               g.attribute("managing_system", "yes");
		if (attr.ld == false)          g.attribute("ld", "no");

		if (attr.location.width()*attr.location.height() > 0)
			g.node("affinity", [&] {
				g.attribute("xpos",   attr.location.xpos());
				g.attribute("ypos",   attr.location.ypos());
				g.attribute("width",  attr.location.width());
				g.attribute("height", attr.location.height()); });

		/* supplement deploy info and diagnostics */
		bool const augment_deploy_info = _blueprint_pkg_path.length() > 1
		                              || option.length() > 1
		                              || _deps == Deps::MISSING
		                              || _deps == Deps::STALLED;
		if (augment_deploy_info)
			g.node("deploy", [&] {

				if (_blueprint_pkg_path.length() > 1)
					g.attribute("pkg", _pkg_path(child));

				if (option.length() > 1)
					g.attribute("option", option);

				if (_pkg == Pkg::DISCOVER) g.node("pkg_undiscovered");
				if (_pkg == Pkg::MISSING)  g.node("pkg_missing");

				if (_pkg == Pkg::CORRUPT)
					g.node("pkg_corrupt", [&] {
						g.attribute("name", _pkg_path(child));
						_for_each_missing_rom([&] (Name const &rom) {
							g.node("missing_rom", [&] {
								g.attribute("name", rom); }); });
					});

				auto gen_missing_dep = [&] (auto const &msg, Node const &conn)
				{
					conn.with_optional_sub_node("child", [&] (Node const &server) {
						auto const res = Resource::from_node(global.resource_types, conn);
						auto const node_type = global.resource_types.node_type(res);
						g.node(msg, [&] {
							g.attribute("name", server.attribute_value("name", Name()));
							g.node(node_type.string(), [&] {
								conn.for_each_attribute([&] (Node::Attribute const &a) {
									/* "service" nodes are deprecated */
									if (conn.type() != "service" || a.name != "name")
										g.attribute(a.name.string(),
										            a.value.start,
										            a.value.num_bytes); }); }); });
					});
				};

				if (_deps == Deps::MISSING || _deps == Deps::STALLED)
					g.tabular_node("deps", [&] {
						_for_each_dep_status(global.child_dict, global.alias_dict,
							[&] (Deps const &deps, Name const &, Node const &conn) {
								if (deps == Deps::MISSING) gen_missing_dep("missing_server", conn);
								if (deps == Deps::STALLED) gen_missing_dep("stalled_server", conn);
						});
					});
			});

		/* lookup if PD/CPU service is configured and use shim in such cases */
		bool shim_reroute = false;
		child.with_optional_sub_node("route", [&] (Node const &route) {
			route.for_each_sub_node("service", [&] (Node const &service) {
				if (service.attribute_value("name", Name()) == "PD" ||
				    service.attribute_value("name", Name()) == "CPU")
					shim_reroute = true; }); });

		Binary_name const binary = shim_reroute ? Binary_name("shim")
		                                        : _binary_name;

		g.node("binary", [&] { g.attribute("name", binary); });

		/*
		 * Insert inline '<heartbeat>' node if provided by the child node
		 */
		if (child.has_sub_node("heartbeat"))
			_gen_copy_of_sub_node(g, child, "heartbeat");

		/*
		 * Unless overridden by a manually routed config, insert inline
		 * '<config>' node if provided by the child node or the blueprint.
		 * The former is preferred over the latter.
		 */
		bool config_defined = _config_overridden(child);

		if (!config_defined && child.has_sub_node("config")) {
			_gen_copy_of_sub_node(g, child, "config");
			config_defined = true; }

		/* runtime handling */
		if (_pkg_node.constructed()) {
			_pkg_node->with_optional_sub_node("runtime", [&] (Node const &runtime) {

				if (!config_defined)
					if (runtime.has_sub_node("config")) {
						_gen_copy_of_sub_node(g, runtime, "config");
						config_defined = true; }

				_gen_provides(g, global.resource_types, runtime);
			});

		} else if (_pkg == Pkg::NONE) {

			_gen_provides(g, global.resource_types, child);
		}

		g.tabular_node("route", [&] {

			if (child.has_sub_node("monitor")) {
				g.node("service", [&] { g.attribute("name", "PD");  g.node("local"); });
				g.node("service", [&] { g.attribute("name", "CPU"); g.node("local"); });
				g.node("service", [&] { g.attribute("name", "VM");  g.node("local"); });
			}

			_gen_routes(g, global, child,
			            _custom_depot_rom.length() > 1 ? _custom_depot_rom
			                                           : global.default_depot_rom);
		});
	});
}


void Depot_deploy::Child::gen_monitor_policy_node(Generator &g) const
{
	_with_node(_child_node, [&] (Node const &child) {

		if (!_discovered(child))
			return;

		if (_pkg != Pkg::NONE && _pkg_node.constructed() && !_pkg_node->has_sub_node("runtime"))
			return;

		child.with_optional_sub_node("monitor", [&] (Node const &monitor) {
			g.node("policy", [&] {
				g.attribute("label", name);
				g.attribute("wait", monitor.attribute_value("wait", false));
				g.attribute("wx",   monitor.attribute_value("wx",   false));
			});
		});
	}, [&] { });
}


void Depot_deploy::Child::_gen_routes(Generator &g,
                                      Global const &global, Node const &child,
                                      Depot_rom_server const &depot_rom) const
{
	if (_pkg != Pkg::NONE && !_pkg_node.constructed())
		return;

	bool route_binary_to_shim = false;

	child.with_optional_sub_node("connect", [&] (Node const &connect) {
		connect.for_each_sub_node([&] (Node const &conn) {
			Resource const resource = Resource::from_node(global.resource_types, conn);
			if (resource.type == Resource::PD)
				route_binary_to_shim = true; }); });

	child.with_optional_sub_node("route", [&] (Node const &route) {
		route.for_each_sub_node([&] (Node const &service) {
			Name const service_name = service.attribute_value("name", Name());
			if (service_name == "PD" || service_name == "CPU")
				route_binary_to_shim = true; }); });

	using Path = String<160>;

	auto copy_route = [&] (Node const &service)
	{
		g.node(service.type().string(), [&] {
			g.node_attributes(service);
			service.for_each_sub_node([&] (Node const &target) {
				g.node(target.type().string(), [&] {
					g.node_attributes(target); }); }); });
	};

	auto route_from_connection = [&] (Node const &conn)
	{
		Resource const resource = Resource::from_node(global.resource_types, conn);
		if (resource.type == Resource::UNDEFINED) {
			warning(name, ": unknown resource type: ", conn);
			return;
		}

		g.node("service", [&] {
			g.attribute("name", resource.service_name());

			using Prefix = String<Resource::Name::capacity() + 3>;
			using Last   = Resource::Name;

			Resource::Name const name = conn.attribute_value("name", Resource::Name());
			bool const named = (name.length() > 1);
			if (named) {
				if (resource.type == Resource::FS)
					g.attribute("label_prefix", Prefix { name, " ->" });
				else if (resource.type == Resource::ROM)
					g.attribute("label_last", name);
				else
					g.attribute("label", name);
			} else {
				if (conn.has_attribute("label_last"))
					g.attribute("label_last",
					            conn.attribute_value("label_last", Last()));
				if (conn.has_attribute("label_prefix"))
					g.attribute("label_prefix",
					            conn.attribute_value("label_prefix", Prefix()));
			}

			if (conn.num_sub_nodes() != 1) {
				warning(name, ": invalid connection: ", conn);
				return;
			}
			conn.for_each_sub_node([&] (Node const &target) {
				g.node(target.type().string(), [&] {
					g.node_attributes(target); }); }); });
	};

	/*
	 * If the subsystem is hosted under a shim, make the shim binary available
	 * and supplement env-session routes for the shim
	 */
	if (route_binary_to_shim) {
		g.node("service", [&] {
			g.attribute("name", "ROM");
			g.attribute("unscoped_label", "shim");
			g.node("parent", [&] { g.attribute("label", "shim"); }); });
		g.node("service", [&] {
			g.attribute("name", "CPU");
			g.attribute("unscoped_label", name);
			g.node("parent", [&] { }); });
		g.node("service", [&] {
			g.attribute("name", "PD");
			g.attribute("unscoped_label", name);
			g.node("parent", [&] { }); });
	}

	/*
	 * Helper for detecting and skipping broken dependencies
	 */
	bool const stalled = !runnable();

	auto target_exists = [&] (Node const &conn)
	{
		if (!stalled) /* cheap check first */
			return true;

		bool result = true;
		conn.for_each_sub_node([&] (Node const &target) {
			Name const &child_or_alias = target.attribute_value("name", Name());
			if (target.has_type("child")) {
				with_server_name(child_or_alias, global.alias_dict, [&] (Name const &name) {
					if (!global.child_dict.exists(name))
						result = false; }); } });
		return result;
	};

	/*
	 * Add routes according to the child's connect node
	 */
	child.with_optional_sub_node("connect", [&] (Node const &connect) {
		connect.for_each_sub_node([&] (Node const &connection) {
			if (target_exists(connection))
				route_from_connection(connection); }); });

	/*
	 * Add routes given in the start node (deprecated)
	 */
	child.with_optional_sub_node("route", [&] (Node const &route) {
		route.for_each_sub_node("service", [&] (Node const &service) {
			if (target_exists(service))
				copy_route(service); }); });

	/*
	 * Redirect config ROM request to label as given in the 'config' attribute,
	 * if present. We need to search the blueprint's <rom> nodes for the
	 * matching ROM module to rewrite the label with the configuration's path
	 * within the depot.
	 */
	if (!_config_overridden(child) && _pkg_node.constructed() && _config_name.valid()) {
		_pkg_node->for_each_sub_node("rom", [&] (Node const &rom) {

			if (!rom.has_attribute("path"))
				return;

			if (rom.attribute_value("name", Name()) != _config_name)
				return;

			/* we found the <rom> node for the config ROM */
			g.node("service", [&] {
				g.attribute("name",  "ROM");
				g.attribute("label", "config");
				using Path = String<160>;
				Path const path = rom.attribute_value("path", Path());

				if (depot_rom.length() > 1)
					g.node("child", [&] {
						g.attribute("name", depot_rom);
						g.attribute("label", path); });
				else
					g.node("parent", [&] {
						g.attribute("label", path); });
			});
		});
	}

	/*
	 * Add common routes as defined in our config.
	 */
	global.common.for_each_sub_node([&] (Node const &service) {
		copy_route(service); });

	/*
	 * Add ROM routing rule with the label rewritten to the path within the
	 * depot.
	 */
	if (_pkg_node.constructed())
		_pkg_node->for_each_sub_node("rom", [&] (Node const &rom) {

			if (!rom.has_attribute("path"))
				return;

			using Label = Name;
			Path  const path = rom.attribute_value("path", Path());
			Label const name = rom.attribute_value("name", Label());
			Label const as   = rom.attribute_value("as",   name);

			/* drop route to "config" ROM if config is manually routed */
			if (as == "config" && _config_overridden(child))
				return;

			g.node("service", [&] {
				g.attribute("name", "ROM");

				if (route_binary_to_shim && name == _binary_name)
					g.attribute("label", "binary");
				else
					g.attribute("label_last", as);

				if (depot_rom.length() > 1) {
					g.node("child", [&] {
						g.attribute("name",  depot_rom);
						g.attribute("label", path);
					});
				} else {
					g.node("parent", [&] {
						g.attribute("label", path); });
				}
			});
		});

	/*
	 * Route to the binary if deployed directly without a pkg
	 */
	if (_pkg == Pkg::NONE)
		g.node("service", [&] {
			g.attribute("name", "ROM");
			g.attribute("label_last", _binary_name);
			g.node("parent", [&] {
				g.attribute("label", _binary_name); });
		});
}

#endif /* _CHILD_H_ */
