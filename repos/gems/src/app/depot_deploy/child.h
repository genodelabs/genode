/*
 * \brief  Child representation
 * \author Norman Feske
 * \date   2018-01-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* Genode includes */
#include <util/list_model.h>
#include <base/service.h>
#include <os/reporter.h>
#include <os/buffered_xml.h>
#include <depot/archive.h>

namespace Depot_deploy {
	using namespace Depot;
	struct Child;
}


class Depot_deploy::Child : public List_model<Child>::Element
{
	public:

		typedef String<100> Name;
		typedef String<80>  Binary_name;
		typedef String<80>  Config_name;
		typedef String<32>  Depot_rom_server;
		typedef String<100> Launcher_name;

		struct Prio_levels
		{
			unsigned value;

			int min_priority() const
			{
				return (value > 0) ? -(int)(value - 1) : 0;
			}
		};

	private:

		Allocator &_alloc;

		Reconstructible<Buffered_xml> _start_xml;         /* from config */
		Constructible<Buffered_xml>   _launcher_xml { };
		Constructible<Buffered_xml>   _pkg_xml { };       /* from blueprint */

		/*
		 * State of the condition check for generating the start node of
		 * the child. I.e., if the child is complete and configured but
		 * a used server component is missing, we need to suppress the start
		 * node until the condition is satisfied.
		 */
		enum Condition { UNCHECKED, SATISFIED, UNSATISFIED };

		Condition _condition { UNCHECKED };

		Name const _name;

		bool _defined_by_launcher() const
		{
			/*
			 * If the <start> node lacks a 'pkg' attribute, we expect the
			 * policy to be defined by a launcher XML snippet.
			 */
			return _start_xml.constructed() && !_start_xml->xml().has_attribute("pkg");
		}

		Archive::Path _config_pkg_path() const
		{
			if (_defined_by_launcher() && _launcher_xml.constructed())
				return _launcher_xml->xml().attribute_value("pkg", Archive::Path());

			return _start_xml->xml().attribute_value("pkg", Archive::Path());
		}

		Launcher_name _launcher_name() const
		{
			if (!_defined_by_launcher())
				return Launcher_name();

			if (_start_xml->xml().has_attribute("launcher"))
				return _start_xml->xml().attribute_value("launcher", Launcher_name());

			return _start_xml->xml().attribute_value("name", Launcher_name());
		}

		/*
		 * The pkg-archive path of the current blueprint query, which may
		 * deviate from pkg path given in the config, once the config is
		 * updated.
		 */
		Archive::Path _blueprint_pkg_path = _config_pkg_path();

		/*
		 * Quota definitions obtained from the blueprint
		 */
		Number_of_bytes _pkg_ram_quota { 0 };
		unsigned long   _pkg_cap_quota { 0 };
		unsigned long   _pkg_cpu_quota { 0 };

		Binary_name _binary_name { };
		Config_name _config_name { };

		/*
		 * Set if the depot query for the child's blueprint failed.
		 */
		enum class State { UNKNOWN, PKG_INCOMPLETE, PKG_COMPLETE };

		State _state = State::UNKNOWN;

		bool _configured() const
		{
			return _pkg_xml.constructed()
			   && (_config_pkg_path() == _blueprint_pkg_path);
		}

		inline void _gen_routes(Xml_generator &, Xml_node,
		                        Depot_rom_server const &,
		                        Depot_rom_server const &) const;

		static void _gen_provides_sub_node(Xml_generator &xml, Xml_node service,
		                                   Xml_node::Type const &node_type,
		                                   Service::Name  const &service_name)
		{
			if (service.type() == node_type)
				xml.node("service", [&] {
					xml.attribute("name", service_name); });
		}

		static void _gen_copy_of_sub_node(Xml_generator &xml, Xml_node from_node,
		                                  Xml_node::Type const &sub_node_type)
		{
			if (!from_node.has_sub_node(sub_node_type.string()))
				return;

			Xml_node const sub_node = from_node.sub_node(sub_node_type.string());
			sub_node.with_raw_node([&] (char const *start, size_t length) {
				xml.append("\n\t\t");
				xml.append(start, length); });
		}

	public:

		Child(Allocator &alloc, Xml_node start_node)
		:
			_alloc(alloc),
			_start_xml(_alloc, start_node),
			_name(_start_xml->xml().attribute_value("name", Name()))
		{ }

		Name name() const { return _name; }

		/*
		 * \return true if config had an effect on the child's state
		 */
		bool apply_config(Xml_node start_node)
		{
			if (!start_node.differs_from(_start_xml->xml()))
				return false;

			Archive::Path const old_pkg_path = _config_pkg_path();

			/* import new start node */
			_start_xml.construct(_alloc, start_node);

			Archive::Path const new_pkg_path = _config_pkg_path();

			/* invalidate blueprint if 'pkg' path changed */
			if (old_pkg_path != new_pkg_path) {
				_blueprint_pkg_path = new_pkg_path;
				_pkg_xml.destruct();

				/* reset error state, attempt to obtain the blueprint again */
				_state = State::UNKNOWN;
			}
			return true;
		}

		/*
		 * \return true if bluerprint had an effect on the child
		 */
		bool apply_blueprint(Xml_node pkg)
		{
			if (_state == State::PKG_COMPLETE)
				return false;

			if (pkg.attribute_value("path", Archive::Path()) != _blueprint_pkg_path)
				return false;

			/* check for the completeness of all ROM ingredients */
			bool any_rom_missing = false;
			pkg.for_each_sub_node("missing_rom", [&] (Xml_node const &missing_rom) {
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
				return (orig_state != _state);
			}

			/* package was missing but is installed now */
			_state = State::PKG_COMPLETE;

			Xml_node const runtime = pkg.sub_node("runtime");

			_pkg_ram_quota = runtime.attribute_value("ram", Number_of_bytes());
			_pkg_cap_quota = runtime.attribute_value("caps", 0UL);
			_pkg_cpu_quota = runtime.attribute_value("cpu", 0UL);

			_binary_name = runtime.attribute_value("binary", Binary_name());
			_config_name = runtime.attribute_value("config", Config_name());

			/* keep copy of the blueprint info */
			_pkg_xml.construct(_alloc, pkg);

			return true;
		}

		bool apply_launcher(Launcher_name const &name, Xml_node launcher)
		{
			if (!_defined_by_launcher())
				return false;

			if (_launcher_name() != name)
				return false;

			if (_launcher_xml.constructed() && !launcher.differs_from(_launcher_xml->xml()))
				return false;

			_launcher_xml.construct(_alloc, launcher);

			_blueprint_pkg_path = _config_pkg_path();

			return true;
		}

		/*
		 * \return true if condition changed
		 */
		bool apply_condition(auto const &cond_fn)
		{
			Condition const orig_condition = _condition;

			Xml_node launcher_xml = _launcher_xml.constructed()
			                      ? _launcher_xml->xml()
			                      : Xml_node("<empty/>");

			if (_start_xml.constructed())
				_condition = cond_fn(_start_xml->xml(), launcher_xml)
				           ? SATISFIED : UNSATISFIED;

			return _condition != orig_condition;
		}

		void apply_if_unsatisfied(auto const &fn) const
		{
			Xml_node launcher_xml = _launcher_xml.constructed()
			                      ? _launcher_xml->xml()
			                      : Xml_node("<empty/>");

			if (_condition == UNSATISFIED && _start_xml.constructed())
				fn(_start_xml->xml(), launcher_xml, _name);
		}

		/*
		 * \return true if the call had an effect on the child
		 */
		bool mark_as_incomplete(Xml_node missing)
		{
			/* print error message only once */
			if(_state == State::PKG_INCOMPLETE)
				return false;

			Archive::Path const path = missing.attribute_value("path", Archive::Path());
			if (path != _blueprint_pkg_path)
				return false;

			log(path, " incomplete or missing");

			State const orig_state = _state;
			_state = State::PKG_INCOMPLETE;

			return (orig_state != _state);
		}

		/**
		 * Reconsider deployment of child after installing missing archives
		 */
		void reset_incomplete()
		{
			if (_state == State::PKG_INCOMPLETE) {
				_state = State::UNKNOWN;
				_pkg_xml.destruct();
			}
		}

		bool blueprint_needed() const
		{
			if (_configured())
				return false;

			if (_defined_by_launcher() && !_launcher_xml.constructed())
				return false;

			return true;
		}

		void gen_query(Xml_generator &xml) const
		{
			if (blueprint_needed())
				xml.node("blueprint", [&] {
					xml.attribute("pkg", _blueprint_pkg_path); });
		}

		/**
		 * Generate start node of init configuration
		 *
		 * \param common              session routes to be added in addition to
		 *                            the ones found in the pkg blueprint
		 * \param cached_depot_rom    name of the server that provides the depot
		 *                            content as ROM modules. If the string is
		 *                            invalid, ROM requests are routed to the
		 *                            parent.
		 * \param uncached_depot_rom  name of the depot-ROM server used to obtain
		 *                            the content of the depot user "local", which
		 *                            is assumed to be mutable
		 */
		inline void gen_start_node(Xml_generator          &,
		                           Xml_node         const &common,
		                           Prio_levels             prio_levels,
		                           Affinity::Space         affinity_space,
		                           Depot_rom_server const &cached_depot_rom,
		                           Depot_rom_server const &uncached_depot_rom) const;

		inline void gen_monitor_policy_node(Xml_generator&) const;

		void with_missing_pkg_path(auto const &fn) const
		{
			if (_state == State::PKG_INCOMPLETE)
				fn(_config_pkg_path());
		}

		/**
		 * Generate installation entry needed for the completion of the child
		 */
		void gen_installation_entry(Xml_generator &xml) const
		{
			with_missing_pkg_path([&] (Archive::Path const &path) {
				xml.node("archive", [&] {
					xml.attribute("path", path);
					xml.attribute("source", "no"); }); });
		}

		bool incomplete() const { return _state == State::PKG_INCOMPLETE; }

		/**
		 * List_model::Element
		 */
		bool matches(Xml_node const &node) const
		{
			return node.attribute_value("name", Child::Name()) == _name;
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return node.has_type("start");
		}
};


void Depot_deploy::Child::gen_start_node(Xml_generator          &xml,
                                         Xml_node         const &common,
                                         Prio_levels      const  prio_levels,
                                         Affinity::Space  const  affinity_space,
                                         Depot_rom_server const &cached_depot_rom,
                                         Depot_rom_server const &uncached_depot_rom) const
{
	if (!_configured() || _condition == UNSATISFIED)
		return;

	if (_defined_by_launcher() && !_launcher_xml.constructed())
		return;

	if (!_pkg_xml->xml().has_sub_node("runtime")) {
		warning("blueprint for '", _name, "' lacks runtime information");
		return;
	}

	Xml_node const launcher_xml = (_defined_by_launcher())
	                            ? _launcher_xml->xml() : Xml_node("<empty/>");

	Xml_node const start_xml = _start_xml->xml();

	xml.node("start", [&] {

		xml.attribute("name", _name);

		{
			unsigned long caps = _pkg_cap_quota;
			if (_defined_by_launcher())
				caps = launcher_xml.attribute_value("caps", caps);
			caps = start_xml.attribute_value("caps", caps);
			xml.attribute("caps", caps);
		}

		{
			typedef String<64> Version;
			Version const version = _start_xml->xml().attribute_value("version", Version());
			if (version.valid())
				xml.attribute("version", version);
		}

		{
			long priority = prio_levels.min_priority();
			if (_defined_by_launcher())
				priority = launcher_xml.attribute_value("priority", priority);
			priority = start_xml.attribute_value("priority", priority);
			if (priority)
				xml.attribute("priority", priority);
		}

		auto permit_managing_system = [&]
		{
			if (start_xml.attribute_value("managing_system", false))
				return true;

			if (_defined_by_launcher())
				if (launcher_xml.attribute_value("managing_system", false))
					return true;

			return false;
		};
		if (permit_managing_system())
			xml.attribute("managing_system", "yes");

		bool shim_reroute = false;

		/* lookup if PD/CPU service is configured and use shim in such cases */
		if (start_xml.has_sub_node("route")) {
			Xml_node const route = start_xml.sub_node("route");

			route.for_each_sub_node("service", [&] (Xml_node const &service) {
				if (service.attribute_value("name", Name()) == "PD" ||
				    service.attribute_value("name", Name()) == "CPU")
					shim_reroute = true; });
		}

		Binary_name const binary = shim_reroute ? Binary_name("shim")
		                                        : _binary_name;

		xml.node("binary", [&] { xml.attribute("name", binary); });

		Number_of_bytes ram = _pkg_ram_quota;
		if (_defined_by_launcher())
			ram = launcher_xml.attribute_value("ram", ram);
		ram = start_xml.attribute_value("ram", ram);

		xml.node("resource", [&] {
			xml.attribute("name", "RAM");
			xml.attribute("quantum", String<32>(ram));
		});

		unsigned long cpu_quota = _pkg_cpu_quota;
		if (_defined_by_launcher())
			cpu_quota = launcher_xml.attribute_value("cpu", cpu_quota);
		cpu_quota = start_xml.attribute_value("cpu", cpu_quota);

		xml.node("resource", [&] {
			xml.attribute("name", "CPU");
			xml.attribute("quantum", cpu_quota);
		});

		/* affinity-location handling */
		bool const affinity_from_launcher = _defined_by_launcher()
		                                 && launcher_xml.has_sub_node("affinity");

		bool const affinity_from_start = start_xml.has_sub_node("affinity");

		if (affinity_from_start || affinity_from_launcher) {

			Affinity::Location location { };

			if (affinity_from_launcher)
				launcher_xml.with_optional_sub_node("affinity", [&] (Xml_node node) {
					location = Affinity::Location::from_xml(affinity_space, node); });

			if (affinity_from_start)
				start_xml.with_optional_sub_node("affinity", [&] (Xml_node node) {
					location = Affinity::Location::from_xml(affinity_space, node); });

			xml.node("affinity", [&] {
				xml.attribute("xpos",   location.xpos());
				xml.attribute("ypos",   location.ypos());
				xml.attribute("width",  location.width());
				xml.attribute("height", location.height());
			});
		}

		/* runtime handling */
		Xml_node const runtime = _pkg_xml->xml().sub_node("runtime");

		/*
		 * Insert inline '<heartbeat>' node if provided by the start node.
		 */
		if (start_xml.has_sub_node("heartbeat"))
			_gen_copy_of_sub_node(xml, start_xml, "heartbeat");

		/*
		 * Insert inline '<config>' node if provided by the start node,
		 * the launcher definition (if a launcher is user), or the
		 * blueprint. The former is preferred over the latter.
		 */
		if (start_xml.has_sub_node("config")) {
			_gen_copy_of_sub_node(xml, start_xml, "config");

		} else {

			if (_defined_by_launcher() && launcher_xml.has_sub_node("config")) {
				_gen_copy_of_sub_node(xml, launcher_xml, "config");
			} else {
				if (runtime.has_sub_node("config"))
					_gen_copy_of_sub_node(xml, runtime, "config");
			}
		}

		/*
		 * Declare services provided by the subsystem.
		 */
		if (runtime.has_sub_node("provides")) {
			xml.node("provides", [&] {
				runtime.sub_node("provides").for_each_sub_node([&] (Xml_node service) {
					_gen_provides_sub_node(xml, service, "audio_in",    "Audio_in");
					_gen_provides_sub_node(xml, service, "audio_out",   "Audio_out");
					_gen_provides_sub_node(xml, service, "block",       "Block");
					_gen_provides_sub_node(xml, service, "file_system", "File_system");
					_gen_provides_sub_node(xml, service, "framebuffer", "Framebuffer");
					_gen_provides_sub_node(xml, service, "input",       "Input");
					_gen_provides_sub_node(xml, service, "event",       "Event");
					_gen_provides_sub_node(xml, service, "log",         "LOG");
					_gen_provides_sub_node(xml, service, "nic",         "Nic");
					_gen_provides_sub_node(xml, service, "uplink",      "Uplink");
					_gen_provides_sub_node(xml, service, "gui",         "Gui");
					_gen_provides_sub_node(xml, service, "gpu",         "Gpu");
					_gen_provides_sub_node(xml, service, "usb",         "Usb");
					_gen_provides_sub_node(xml, service, "report",      "Report");
					_gen_provides_sub_node(xml, service, "rom",         "ROM");
					_gen_provides_sub_node(xml, service, "terminal",    "Terminal");
					_gen_provides_sub_node(xml, service, "timer",       "Timer");
					_gen_provides_sub_node(xml, service, "pd",          "PD");
					_gen_provides_sub_node(xml, service, "cpu",         "CPU");
					_gen_provides_sub_node(xml, service, "rtc",         "Rtc");
					_gen_provides_sub_node(xml, service, "capture",     "Capture");
				});
			});
		}

		xml.node("route", [&] {

			if (start_xml.has_sub_node("monitor")) {
				xml.node("service", [&] {
					xml.attribute("name", "PD");
					xml.node("local");
				});
				xml.node("service", [&] {
					xml.attribute("name", "CPU");
					xml.node("local");
				});
			}

			_gen_routes(xml, common, cached_depot_rom, uncached_depot_rom);
		});
	});
}


void Depot_deploy::Child::gen_monitor_policy_node(Xml_generator &xml) const
{
	if (!_configured() || _condition == UNSATISFIED)
		return;

	if (_defined_by_launcher() && !_launcher_xml.constructed())
		return;

	if (!_pkg_xml->xml().has_sub_node("runtime")) {
		return;
	}

	Xml_node const start_xml = _start_xml->xml();

	if (start_xml.has_sub_node("monitor")) {
		Xml_node const monitor = start_xml.sub_node("monitor");
		xml.node("policy", [&] {
			xml.attribute("label", _name);
			xml.attribute("wait", monitor.attribute_value("wait", false));
			xml.attribute("wx", monitor.attribute_value("wx", false));
		});
	}
}


void Depot_deploy::Child::_gen_routes(Xml_generator &xml, Xml_node common,
                                      Depot_rom_server const &cached_depot_rom,
                                      Depot_rom_server const &uncached_depot_rom) const
{
	bool route_binary_to_shim = false;

	if (!_pkg_xml.constructed())
		return;

	typedef String<160> Path;

	/*
	 * Add routes given in the start node.
	 */
	if (_start_xml->xml().has_sub_node("route")) {
		Xml_node const route = _start_xml->xml().sub_node("route");

		route.for_each_sub_node("service", [&] (Xml_node const &service) {
			Name const service_name = service.attribute_value("name", Name());

			/* supplement env-session routes for the shim */
			if (service_name == "PD" || service_name == "CPU") {
				route_binary_to_shim = true;

				xml.node("service", [&] {
					xml.attribute("name", service_name);
					xml.attribute("unscoped_label", _name);
					xml.node("parent", [&] { });
				});
			}

			service.with_raw_node([&] (char const *start, size_t length) {
				xml.append("\n\t\t\t");
				xml.append(start, length);
			});
		});
	}

	/*
	 * If the subsystem is hosted under a shim, make the shim binary available
	 */
	if (route_binary_to_shim)
		xml.node("service", [&] {
			xml.attribute("name", "ROM");
			xml.attribute("unscoped_label", "shim");
			xml.node("parent", [&] {
				xml.attribute("label", "shim"); }); });

	/*
	 * Add routes given in the launcher definition.
	 */
	if (_launcher_xml.constructed() && _launcher_xml->xml().has_sub_node("route")) {
		Xml_node const route = _launcher_xml->xml().sub_node("route");
		route.with_raw_content([&] (char const *start, size_t length) {
			xml.append(start, length); });
	}

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
	if (_config_name.valid()) {
		_pkg_xml->xml().for_each_sub_node("rom", [&] (Xml_node rom) {

			if (!rom.has_attribute("path"))
				return;

			if (rom.attribute_value("label", Name()) != _config_name)
				return;

			/* we found the <rom> node for the config ROM */
			xml.node("service", [&] {
				xml.attribute("name",  "ROM");
				xml.attribute("label", "config");
				typedef String<160> Path;
				Path const path = rom.attribute_value("path", Path());

				if (cached_depot_rom.valid())
					xml.node("child", [&] {
						xml.attribute("name", rom_server(path));
						xml.attribute("label", path); });
				else
					xml.node("parent", [&] {
						xml.attribute("label", path); });
			});
		});
	}

	/*
	 * Add common routes as defined in our config.
	 */
	common.with_raw_content([&] (char const *start, size_t length) {
		xml.append(start, length); });

	/*
	 * Add ROM routing rule with the label rewritten to the path within the
	 * depot.
	 */
	_pkg_xml->xml().for_each_sub_node("rom", [&] (Xml_node rom) {

		if (!rom.has_attribute("path"))
			return;

		typedef Name Label;
		Path  const path  = rom.attribute_value("path",  Path());
		Label const label = rom.attribute_value("label", Label());
		Label const as    = rom.attribute_value("as",    label);

		xml.node("service", [&] {
			xml.attribute("name", "ROM");

			if (route_binary_to_shim && label == _binary_name)
				xml.attribute("label", "binary");
			else
				xml.attribute("label_last", as);

			if (cached_depot_rom.valid()) {
				xml.node("child", [&] {
					xml.attribute("name",  rom_server(path));
					xml.attribute("label", path);
				});
			} else {
				xml.node("parent", [&] {
					xml.attribute("label", path); });
			}
		});
	});
}

#endif /* _CHILD_H_ */
