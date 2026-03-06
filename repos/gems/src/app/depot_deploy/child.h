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
#include <base/service.h>
#include <base/node.h>
#include <os/reporter.h>
#include <depot/archive.h>

namespace Depot_deploy {

	using namespace Depot;

	static constexpr Generator::Max_depth MAX_NODE_DEPTH = { 20 };

	struct Child;

	using Child_name = String<100>;
	using Dictionary = Dictionary<Child, Child_name>;

	struct Duplicate_checked
	{
		bool const duplicate;

		Duplicate_checked(Dictionary &dict, Child_name const &name)
		:
			duplicate(dict.exists(name))
		{
			if (duplicate)
				warning("omitting child with duplicated name '", name, "'");
		}
	};
}


class Depot_deploy::Child : public Duplicate_checked,
                            public List_model<Child>::Element,
                            public Dictionary::Element
{
	public:

		using Name             = Child_name;
		using Binary_name      = String<80>;
		using Config_name      = String<80>;
		using Depot_rom_server = String<32>;
		using Launcher_name    = String<100>;

		struct Prio_levels
		{
			unsigned value;

			int min_priority() const
			{
				return (value > 0) ? -(int)(value - 1) : 0;
			}
		};

		static Name node_name(Node const &node)
		{
			return node.attribute_value("name", Name());
		}

	private:

		Constructible<Buffered_node> _start_node    { };  /* from config */
		Constructible<Buffered_node> _launcher_node { };
		Constructible<Buffered_node> _pkg_node      { };  /* from blueprint */

		static auto _with_node(auto &constructible_node,
		                       auto const &fn, auto const &missing_fn) -> decltype(missing_fn())
		{
			if (!constructible_node.constructed())
				return missing_fn();

			Node const &node = *constructible_node;
			return fn(node);
		}

		static auto _identical(auto &constructible_node, Node const &node)
		{
			return _with_node(constructible_node,
				[&] (Node const &orig) { return !node.differs_from(orig); },
				[&]                    { return false; });
		}

		/*
		 * State of the condition check for generating the start node of
		 * the child. I.e., if the child is complete and configured but
		 * a used server component is missing, we need to suppress the start
		 * node until the condition is satisfied.
		 */
		enum Condition { UNCHECKED, SATISFIED, UNSATISFIED };

		Condition _condition { UNCHECKED };

		bool _defined_by_launcher(Node const &start_node) const
		{
			return !start_node.has_attribute("pkg")
			    && !start_node.has_attribute("binary");
		}

		Archive::Path _config_pkg_path(Node const &start_node) const
		{
			if (_defined_by_launcher(start_node) && _launcher_node.constructed())
				return _launcher_node->attribute_value("pkg", Archive::Path());

			return start_node.attribute_value("pkg", Archive::Path());
		}

		Launcher_name _launcher_name(Node const &start_node) const
		{
			if (!_defined_by_launcher(start_node))
				return Launcher_name();

			if (start_node.has_attribute("launcher"))
				return start_node.attribute_value("launcher", Launcher_name());

			return start_node.attribute_value("name", Launcher_name());
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
		Number_of_bytes _pkg_ram_quota { 0 };
		unsigned long   _pkg_cap_quota { 0 };

		Binary_name _binary_name { };
		Config_name _config_name { };

		/*
		 * Set if the depot query for the child's blueprint failed.
		 */
		enum class State { UNKNOWN, NO_PKG, PKG_INCOMPLETE, PKG_COMPLETE };

		State _state = State::UNKNOWN;

		bool _discovered(Node const &start_node) const
		{
			if (_state == State::NO_PKG)
				return true;

			return _pkg_node.constructed()
			   && (_config_pkg_path(start_node) == _blueprint_pkg_path);
		}

		inline void _gen_routes(Generator &, Node const &, Node const &,
		                        Depot_rom_server const &,
		                        Depot_rom_server const &) const;

		static void _gen_provides_sub_node(Generator &g, Node const &service,
		                                   Node::Type const &node_type,
		                                   Service::Name  const &service_name)
		{
			if (service.type() == node_type)
				g.node("service", [&] {
					g.attribute("name", service_name); });
		}

		/*
		 * node is pkg runtime or deploy start (NO_PKG)
		 */
		static void _gen_provides(Generator &g, Node const &node)
		{
			node.with_optional_sub_node("provides", [&] (Node const &provides) {
				g.node("provides", [&] {
					provides.for_each_sub_node([&] (Node const &service) {
						_gen_provides_sub_node(g, service, "audio_in",    "Audio_in");
						_gen_provides_sub_node(g, service, "audio_out",   "Audio_out");
						_gen_provides_sub_node(g, service, "block",       "Block");
						_gen_provides_sub_node(g, service, "file_system", "File_system");
						_gen_provides_sub_node(g, service, "framebuffer", "Framebuffer");
						_gen_provides_sub_node(g, service, "input",       "Input");
						_gen_provides_sub_node(g, service, "event",       "Event");
						_gen_provides_sub_node(g, service, "log",         "LOG");
						_gen_provides_sub_node(g, service, "nic",         "Nic");
						_gen_provides_sub_node(g, service, "uplink",      "Uplink");
						_gen_provides_sub_node(g, service, "gui",         "Gui");
						_gen_provides_sub_node(g, service, "gpu",         "Gpu");
						_gen_provides_sub_node(g, service, "usb",         "Usb");
						_gen_provides_sub_node(g, service, "report",      "Report");
						_gen_provides_sub_node(g, service, "rom",         "ROM");
						_gen_provides_sub_node(g, service, "terminal",    "Terminal");
						_gen_provides_sub_node(g, service, "timer",       "Timer");
						_gen_provides_sub_node(g, service, "pd",          "PD");
						_gen_provides_sub_node(g, service, "cpu",         "CPU");
						_gen_provides_sub_node(g, service, "rtc",         "Rtc");
						_gen_provides_sub_node(g, service, "capture",     "Capture");
						_gen_provides_sub_node(g, service, "play",        "Play");
						_gen_provides_sub_node(g, service, "record",      "Record");
					});
				});
			});
		}

		static void _gen_copy_of_sub_node(Generator &g, Node const &from_node,
		                                  Node::Type const &sub_node_type)
		{
			from_node.with_optional_sub_node(sub_node_type.string(),
				[&] (Node const &sub_node) {
					if (!g.append_node(sub_node, MAX_NODE_DEPTH))
						warning("sub node exceeds max depth: ", from_node); });
		}

		inline void _gen_start_node(Generator              &,
		                            Node             const &common,
		                            Node             const &start_node,
		                            Prio_levels             prio_levels,
		                            Affinity::Space         affinity_space,
		                            Depot_rom_server const &cached_depot_rom,
		                            Depot_rom_server const &uncached_depot_rom) const;

	public:

		Child(Dictionary &dict, Name const &name)
		:
			Duplicate_checked(dict, name), Dictionary::Element(dict, name)
		{ }

		Progress apply_config(Allocator &alloc, Node const &start_node)
		{
			if (_identical(_start_node, start_node))
				return STALLED;

			Archive::Path const orig_pkg_path = _blueprint_pkg_path;

			/* import new start node */
			_start_node.construct(alloc, start_node);

			_blueprint_pkg_path = _config_pkg_path(start_node);

			/* invalidate blueprint if 'pkg' path changed */
			if (orig_pkg_path != _blueprint_pkg_path) {
				_pkg_node.destruct();

				/* reset error state, attempt to obtain the blueprint again */
				_state = State::UNKNOWN;
			}

			if (start_node.has_attribute("binary")) {
				_state = State::NO_PKG;
				_pkg_node.destruct();
				_binary_name = start_node.attribute_value("binary", Binary_name());
				_config_name = start_node.attribute_value("config", Config_name());
			}

			return PROGRESSED;
		}

		/*
		 * \return true if bluerprint had an effect on the child
		 */
		Progress apply_blueprint(Allocator &alloc, Node const &pkg)
		{
			if (_state == State::PKG_COMPLETE || _state == State::NO_PKG)
				return STALLED;

			if (pkg.attribute_value("path", Archive::Path()) != _blueprint_pkg_path)
				return STALLED;

			/* check for the completeness of all ROM ingredients */
			bool any_rom_missing = false;
			pkg.for_each_sub_node("missing_rom", [&] (Node const &missing_rom) {
				Name const name = missing_rom.attribute_value("label", Name());

				/* ld.lib.so is special because it is provided by the base system */
				if (name == "ld.lib.so")
					return;

				warning("missing ROM module '", name, "' needed by ", _blueprint_pkg_path);
				any_rom_missing = true;
			});

			if (any_rom_missing) {
				State const orig_state = _state;
				_state = State::PKG_INCOMPLETE;
				return { .progressed = (orig_state != _state) };
			}

			/* package was missing but is installed now */
			_state = State::PKG_COMPLETE;

			pkg.with_sub_node("runtime",
				[&] (Node const &runtime) {

					_pkg_ram_quota = runtime.attribute_value("ram", Number_of_bytes());
					_pkg_cap_quota = runtime.attribute_value("caps", 0UL);

					_binary_name = runtime.attribute_value("binary", Binary_name());
					_config_name = runtime.attribute_value("config", Config_name());

					/* keep copy of the blueprint info */
					_pkg_node.construct(alloc, pkg);
				},
				[&] { });

			return PROGRESSED;
		}

		Progress apply_launcher(Allocator &alloc,
		                        Launcher_name const &name, Node const &launcher)
		{
			return _with_node(_start_node, [&] (Node const &start_node) {

				if (!_defined_by_launcher(start_node))    return STALLED;
				if (_launcher_name(start_node) != name)   return STALLED;
				if (_identical(_launcher_node, launcher)) return STALLED;

				_launcher_node.construct(alloc, launcher);

				_blueprint_pkg_path = _config_pkg_path(start_node);

				return PROGRESSED;

			}, [&] { return STALLED; });
		}

		Progress apply_condition(auto const &cond_fn)
		{
			Condition const orig_condition = _condition;

			_with_node(_start_node, [&] (Node const &start) {
				bool const satisfied = _with_node(_launcher_node,
					[&] (Node const &launcher) { return cond_fn(start, launcher); },
					[&]                        { return cond_fn(start, Node()); });
				_condition = satisfied ? SATISFIED : UNSATISFIED;
			}, [&] { });
			return { .progressed = (_condition != orig_condition) };
		}

		void apply_if_unsatisfied(auto const &fn) const
		{
			if (_condition == UNSATISFIED)
				_with_node(_start_node, [&] (Node const &start_node) {
					_with_node(_launcher_node,
						[&] (Node const &launcher) { fn(start_node, launcher, name); },
						[&]                        { fn(start_node, Node(),   name); });
				}, [&] { });
		}

		Progress mark_as_incomplete(Node const &missing)
		{
			if (_state == State::NO_PKG)
				return STALLED;

			/* print error message only once */
			if (_state == State::PKG_INCOMPLETE)
				return STALLED;

			Archive::Path const path = missing.attribute_value("path", Archive::Path());
			if (path != _blueprint_pkg_path)
				return STALLED;

			log(path, " incomplete or missing, name=", name, " state=", (int)_state);

			State const orig_state = _state;
			_state = State::PKG_INCOMPLETE;

			return { .progressed = (orig_state != _state) };
		}

		/**
		 * Reconsider deployment of child after installing missing archives
		 */
		void reset_incomplete()
		{
			if (_state == State::PKG_INCOMPLETE) {
				_state = State::UNKNOWN;
				_pkg_node.destruct();
			}
		}

		bool blueprint_needed() const
		{
			if (_state == State::NO_PKG)
				return false;

			return _with_node(_start_node, [&] (Node const &start_node) {

				if (_discovered(start_node))
					return false;

				if (_defined_by_launcher(start_node) && !_launcher_node.constructed())
					return false;

				return true;

			}, [&] { return false; });
		}

		void gen_query(Generator &g) const
		{
			if (blueprint_needed())
				g.node("blueprint", [&] {
					g.attribute("pkg", _blueprint_pkg_path); });
		}

		/**
		 * Generate start node of init configuration
		 *
		 * \param common              session routes to be added in addition to
		 *                            the ones found in the pkg blueprint
		 * \param start_node          start node of deploy config
		 * \param cached_depot_rom    name of the server that provides the depot
		 *                            content as ROM modules. If the string is
		 *                            invalid, ROM requests are routed to the
		 *                            parent.
		 * \param uncached_depot_rom  name of the depot-ROM server used to obtain
		 *                            the content of the depot user "local", which
		 *                            is assumed to be mutable
		 */
		void gen_start_node(Generator              &g,
		                    Node             const &common,
		                    Prio_levels             prio_levels,
		                    Affinity::Space         affinity_space,
		                    Depot_rom_server const &cached_depot_rom,
		                    Depot_rom_server const &uncached_depot_rom) const
		{
			_with_node(_start_node, [&] (Node const &start_node) {
				_gen_start_node(g, common, start_node, prio_levels, affinity_space,
				                cached_depot_rom, uncached_depot_rom);
			}, [&] { });
		}

		inline void gen_monitor_policy_node(Generator &) const;

		void with_missing_pkg_path(auto const &fn) const
		{
			_with_node(_start_node, [&] (Node const &start_node) {
				if (_state == State::PKG_INCOMPLETE)
					fn(_config_pkg_path(start_node));
			}, [&] { });
		}

		/**
		 * Generate installation entry needed for the completion of the child
		 */
		void gen_installation_entry(Generator &g) const
		{
			with_missing_pkg_path([&] (Archive::Path const &path) {
				g.node("archive", [&] {
					g.attribute("path", path);
					g.attribute("source", "no"); }); });
		}

		bool incomplete() const { return _state == State::PKG_INCOMPLETE; }

		/**
		 * List_model::Element
		 */
		bool matches(Node const &node) const { return node_name(node) == name; }

		/**
		 * List_model::Element
		 */
		static bool type_matches(Node const &node) { return node.has_type("start"); }
};


void Depot_deploy::Child::_gen_start_node(Generator              &g,
                                          Node             const &common,
                                          Node             const &start_node,
                                          Prio_levels      const  prio_levels,
                                          Affinity::Space  const  affinity_space,
                                          Depot_rom_server const &cached_depot_rom,
                                          Depot_rom_server const &uncached_depot_rom) const
{
	if (!_discovered(start_node) || _condition == UNSATISFIED)
		return;

	if (_defined_by_launcher(start_node) && !_launcher_node.constructed())
		return;

	if (_pkg_node.constructed() && !_pkg_node->has_sub_node("runtime")) {
		warning("blueprint for '", name, "' lacks runtime information");
		return;
	}

	g.node("start", [&] {

		g.attribute("name", name);

		struct Attr
		{
			using Version = String<64>;

			unsigned long      caps;
			Number_of_bytes    ram;
			Version            version;
			long               priority;
			bool               system;
			Affinity::Location location;

			void apply(Node const &node, Affinity::Space const &affinity_space)
			{
				caps     = node.attribute_value("caps",            caps);
				ram      = node.attribute_value("ram",             ram);
				version  = node.attribute_value("version",         version);
				priority = node.attribute_value("priority",        priority);
				system   = node.attribute_value("managing_system", system);

				node.with_optional_sub_node("affinity", [&] (Node const &node) {
					location = Affinity::Location::from_node(affinity_space, node); });
			}
		};

		Attr attr {
			.caps     = _pkg_cap_quota,
			.ram      = _pkg_ram_quota,
			.version  = { },
			.priority = prio_levels.min_priority(),
			.system   = false,
			.location = { },
		};

		/* launcher override pkg runtime */
		_with_node(_launcher_node, [&] (Node const &launcher) {
			attr.apply(launcher, affinity_space); }, [] { });

		/* start node overrides launcher and pkg runtime */
		attr.apply(start_node, affinity_space);

		g.attribute("ram",  attr.ram);
		g.attribute("caps", attr.caps);
		if (attr.version.length() > 1) g.attribute("version",  attr.version);
		if (attr.priority)             g.attribute("priority", attr.priority);
		if (attr.system)               g.attribute("managing_system", "yes");

		if (attr.location.width()*attr.location.height() > 0)
			g.node("affinity", [&] {
				g.attribute("xpos",   attr.location.xpos());
				g.attribute("ypos",   attr.location.ypos());
				g.attribute("width",  attr.location.width());
				g.attribute("height", attr.location.height()); });

		/* lookup if PD/CPU service is configured and use shim in such cases */
		bool shim_reroute = false;
		start_node.with_optional_sub_node("route", [&] (Node const &route) {
			route.for_each_sub_node("service", [&] (Node const &service) {
				if (service.attribute_value("name", Name()) == "PD" ||
				    service.attribute_value("name", Name()) == "CPU")
					shim_reroute = true; }); });

		Binary_name const binary = shim_reroute ? Binary_name("shim")
		                                        : _binary_name;

		g.node("binary", [&] { g.attribute("name", binary); });

		/* affinity-location handling */
		bool affinity_from_launcher = false;
		_with_node(_launcher_node, [&] (Node const &launcher) {
			affinity_from_launcher = launcher.has_sub_node("affinity"); }, [&] { });

		/*
		 * Insert inline '<heartbeat>' node if provided by the start node
		 * or launcher.
		 */
		if (start_node.has_sub_node("heartbeat"))
			_gen_copy_of_sub_node(g, start_node, "heartbeat");
		else
			_with_node(_launcher_node, [&] (Node const &launcher) {
				if (launcher.has_sub_node("heartbeat"))
					_gen_copy_of_sub_node(g, launcher, "heartbeat"); }, [&] { });

		/*
		 * Insert inline '<config>' node if provided by the start node,
		 * the launcher definition (if a launcher is used), or the
		 * blueprint. The former is preferred over the latter.
		 */
		bool config_defined = false;
		if (start_node.has_sub_node("config")) {
			_gen_copy_of_sub_node(g, start_node, "config");
			config_defined = true; }

		if (!config_defined)
			_with_node(_launcher_node, [&] (Node const &launcher) {
				if (launcher.has_sub_node("config")) {
					_gen_copy_of_sub_node(g, launcher, "config");
					config_defined = true; } }, [&] { });

		/* runtime handling */
		if (_pkg_node.constructed()) {
			_pkg_node->with_optional_sub_node("runtime", [&] (Node const &runtime) {

				if (!config_defined)
					if (runtime.has_sub_node("config")) {
						_gen_copy_of_sub_node(g, runtime, "config");
						config_defined = true; }

				_gen_provides(g, runtime);
			});

		} else if (_state == State::NO_PKG) {

			_gen_provides(g, start_node);
		}

		g.tabular_node("route", [&] {

			if (start_node.has_sub_node("monitor")) {
				g.node("service", [&] {
					g.attribute("name", "PD");
					g.node("local");
				});
				g.node("service", [&] {
					g.attribute("name", "CPU");
					g.node("local");
				});
				g.node("service", [&] {
					g.attribute("name", "VM");
					g.node("local");
				});
			}

			_gen_routes(g, common, start_node, cached_depot_rom, uncached_depot_rom);
		});
	});
}


void Depot_deploy::Child::gen_monitor_policy_node(Generator &g) const
{
	_with_node(_start_node, [&] (Node const &start_node) {

		if (!_discovered(start_node) || _condition == UNSATISFIED)
			return;

		if (_defined_by_launcher(start_node) && !_launcher_node.constructed())
			return;

		if (_state != State::NO_PKG && !_pkg_node->has_sub_node("runtime")) {
			return;
		}

		start_node.with_optional_sub_node("monitor", [&] (Node const &monitor) {
			g.node("policy", [&] {
				g.attribute("label", name);
				g.attribute("wait", monitor.attribute_value("wait", false));
				g.attribute("wx",   monitor.attribute_value("wx",   false));
			});
		});
	}, [&] { });
}


void Depot_deploy::Child::_gen_routes(Generator &g,
                                      Node const &common,
                                      Node const &start_node,
                                      Depot_rom_server const &cached_depot_rom,
                                      Depot_rom_server const &uncached_depot_rom) const
{
	bool route_binary_to_shim = false;

	if (_state != State::NO_PKG && !_pkg_node.constructed())
		return;

	using Path = String<160>;

	auto copy_route = [&] (Node const &service)
	{
		g.node(service.type().string(), [&] {
			g.node_attributes(service);
			service.for_each_sub_node([&] (Node const &target) {
				g.node(target.type().string(), [&] {
					g.node_attributes(target); }); }); });
	};

	/*
	 * Add routes given in the start node.
	 */
	start_node.with_optional_sub_node("route", [&] (Node const &route) {

		route.for_each_sub_node("service", [&] (Node const &service) {
			Name const service_name = service.attribute_value("name", Name());

			/* supplement env-session routes for the shim */
			if (service_name == "PD" || service_name == "CPU") {
				route_binary_to_shim = true;

				g.node("service", [&] {
					g.attribute("name", service_name);
					g.attribute("unscoped_label", name);
					g.node("parent", [&] { });
				});
			}

			copy_route(service);
		});
	});

	/*
	 * If the subsystem is hosted under a shim, make the shim binary available
	 */
	if (route_binary_to_shim)
		g.node("service", [&] {
			g.attribute("name", "ROM");
			g.attribute("unscoped_label", "shim");
			g.node("parent", [&] {
				g.attribute("label", "shim"); }); });

	/*
	 * Add routes given in the launcher definition.
	 */
	_with_node(_launcher_node, [&] (Node const &launcher) {
		launcher.with_optional_sub_node("route", [&] (Node const &route) {
			route.for_each_sub_node([&] (Node const &service) {
				copy_route(service); }); }); }, [&] { });

	/**
	 * Return name of depot-ROM server used for obtaining the 'path'
	 *
	 * If the depot path refers to the depot-user "local", route the
	 * session request to the non-cached ROM service.
	 */
	auto rom_server = [&] (Path const &path) {

		return (String<7>(path) == "local/") ? uncached_depot_rom
		                                     : cached_depot_rom;
	};

	/*
	 * Redirect config ROM request to label as given in the 'config' attribute,
	 * if present. We need to search the blueprint's <rom> nodes for the
	 * matching ROM module to rewrite the label with the configuration's path
	 * within the depot.
	 */
	if (_pkg_node.constructed() && _config_name.valid()) {
		_pkg_node->for_each_sub_node("rom", [&] (Node const &rom) {

			if (!rom.has_attribute("path"))
				return;

			if (rom.attribute_value("label", Name()) != _config_name)
				return;

			/* we found the <rom> node for the config ROM */
			g.node("service", [&] {
				g.attribute("name",  "ROM");
				g.attribute("label", "config");
				using Path = String<160>;
				Path const path = rom.attribute_value("path", Path());

				if (cached_depot_rom.valid())
					g.node("child", [&] {
						g.attribute("name", rom_server(path));
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
	common.for_each_sub_node([&] (Node const &service) {
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
			Path  const path  = rom.attribute_value("path",  Path());
			Label const label = rom.attribute_value("label", Label());
			Label const as    = rom.attribute_value("as",    label);

			g.node("service", [&] {
				g.attribute("name", "ROM");

				if (route_binary_to_shim && label == _binary_name)
					g.attribute("label", "binary");
				else
					g.attribute("label_last", as);

				if (cached_depot_rom.valid()) {
					g.node("child", [&] {
						g.attribute("name",  rom_server(path));
						g.attribute("label", path);
					});
				} else {
					g.node("parent", [&] {
						g.attribute("label", path); });
				}
			});
		});

	/*
	 * Copy routes of start node deployed directly without a pkg
	 */
	if (_state == State::NO_PKG)
		g.node("service", [&] {
			g.attribute("name", "ROM");
			g.attribute("label_last", _binary_name);
			g.node("parent", [&] {
				g.attribute("label", _binary_name); });
		});
}

#endif /* _CHILD_H_ */
