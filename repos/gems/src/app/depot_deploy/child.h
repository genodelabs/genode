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

		Binary_name _binary_name { };
		Config_name _config_name { };

		/*
		 * Set if the depot query for the child's blueprint failed.
		 */
		bool _pkg_incomplete = false;

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
				xml.node("service", [&] () {
					xml.attribute("name", service_name); });
		}

		static void _gen_copy_of_sub_node(Xml_generator &xml, Xml_node from_node,
		                                  Xml_node::Type const &sub_node_type)
		{
			if (!from_node.has_sub_node(sub_node_type.string()))
				return;

			Xml_node const sub_node = from_node.sub_node(sub_node_type.string());
			sub_node.with_raw_node([&] (char const *start, size_t length) {
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

		void apply_config(Xml_node start_node)
		{
			if (!start_node.differs_from(_start_xml->xml()))
				return;

			Archive::Path const old_pkg_path = _config_pkg_path();

			/* import new start node */
			_start_xml.construct(_alloc, start_node);

			Archive::Path const new_pkg_path = _config_pkg_path();

			/* invalidate blueprint if 'pkg' path changed */
			if (old_pkg_path != new_pkg_path) {
				_blueprint_pkg_path = new_pkg_path;
				_pkg_xml.destruct();

				/* reset error state, attempt to obtain the blueprint again */
				_pkg_incomplete = false;
			}
		}

		void apply_blueprint(Xml_node pkg)
		{
			if (pkg.attribute_value("path", Archive::Path()) != _blueprint_pkg_path)
				return;

			/* package was missing but is installed now */
			_pkg_incomplete = false;

			Xml_node const runtime = pkg.sub_node("runtime");

			_pkg_ram_quota = runtime.attribute_value("ram", Number_of_bytes());
			_pkg_cap_quota = runtime.attribute_value("caps", 0UL);

			_binary_name = runtime.attribute_value("binary", Binary_name());
			_config_name = runtime.attribute_value("config", Config_name());

			/* keep copy of the blueprint info */
			_pkg_xml.construct(_alloc, pkg);
		}

		void apply_launcher(Launcher_name const &name, Xml_node launcher)
		{
			if (!_defined_by_launcher())
				return;

			if (_launcher_name() != name)
				return;

			if (_launcher_xml.constructed() && !launcher.differs_from(_launcher_xml->xml()))
				return;

			_launcher_xml.construct(_alloc, launcher);

			_blueprint_pkg_path = _config_pkg_path();
		}

		/*
		 * \return true if condition changed
		 */
		template <typename COND_FN>
		bool apply_condition(COND_FN const &fn)
		{
			Condition const orig_condition = _condition;

			Xml_node launcher_xml = _launcher_xml.constructed()
			                      ? _launcher_xml->xml()
			                      : Xml_node("<empty/>");

			if (_start_xml.constructed())
				_condition = fn(_start_xml->xml(), launcher_xml)
				           ? SATISFIED : UNSATISFIED;

			return _condition != orig_condition;
		}

		template <typename FN>
		void apply_if_unsatisfied(FN const &fn) const
		{
			Xml_node launcher_xml = _launcher_xml.constructed()
			                      ? _launcher_xml->xml()
			                      : Xml_node("<empty/>");

			if (_condition == UNSATISFIED && _start_xml.constructed())
				fn(_start_xml->xml(), launcher_xml, _name);
		}

		void mark_as_incomplete(Xml_node missing)
		{
			/* print error message only once */
			if(_pkg_incomplete)
				return;

			Archive::Path const path = missing.attribute_value("path", Archive::Path());
			if (path != _blueprint_pkg_path)
				return;

			log(path, " incomplete or missing");

			_pkg_incomplete = true;
		}

		/**
		 * Reconsider deployment of child after installing missing archives
		 */
		void reset_incomplete()
		{
			if (_pkg_incomplete) {
				_pkg_incomplete = false;
				_pkg_xml.destruct();
			}
		}

		bool blueprint_needed() const
		{
			if (_configured() || _pkg_incomplete)
				return false;

			if (_defined_by_launcher() && !_launcher_xml.constructed())
				return false;

			return true;
		}

		void gen_query(Xml_generator &xml) const
		{
			if (blueprint_needed())
				xml.node("blueprint", [&] () {
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
		inline void gen_start_node(Xml_generator &, Xml_node common,
		                           Depot_rom_server const &cached_depot_rom,
		                           Depot_rom_server const &uncached_depot_rom) const;

		/**
		 * Generate installation entry needed for the completion of the child
		 */
		void gen_installation_entry(Xml_generator &xml) const
		{
			if (!_pkg_incomplete) return;

			xml.node("archive", [&] () {
				xml.attribute("path", _config_pkg_path());
				xml.attribute("source", "no");
			});
		}

		bool incomplete() const { return _pkg_incomplete; }
};


void Depot_deploy::Child::gen_start_node(Xml_generator &xml, Xml_node common,
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

	xml.node("start", [&] () {

		xml.attribute("name", _name);

		unsigned long caps = _pkg_cap_quota;
		if (_defined_by_launcher())
			caps = _launcher_xml->xml().attribute_value("caps", caps);
		caps = _start_xml->xml().attribute_value("caps", caps);

		xml.attribute("caps", caps);

		typedef String<64> Version;
		Version const version = _start_xml->xml().attribute_value("version", Version());
		if (version.valid())
			xml.attribute("version", version);

		xml.node("binary", [&] () { xml.attribute("name", _binary_name); });

		Number_of_bytes ram = _pkg_ram_quota;
		if (_defined_by_launcher())
			ram = _launcher_xml->xml().attribute_value("ram", ram);
		ram = _start_xml->xml().attribute_value("ram", ram);

		xml.node("resource", [&] () {
			xml.attribute("name", "RAM");
			xml.attribute("quantum", String<32>(ram));
		});

		Xml_node const runtime = _pkg_xml->xml().sub_node("runtime");

		/*
		 * Insert inline '<config>' node if provided by the start node,
		 * the launcher definition (if a launcher is user), or the
		 * blueprint. The former is preferred over the latter.
		 */
		if (_start_xml->xml().has_sub_node("config")) {
			_gen_copy_of_sub_node(xml, _start_xml->xml(), "config");
		} else {
			if (_defined_by_launcher() && _launcher_xml->xml().has_sub_node("config")) {
				_gen_copy_of_sub_node(xml, _launcher_xml->xml(), "config");
			} else {
				if (runtime.has_sub_node("config"))
					_gen_copy_of_sub_node(xml, runtime, "config");
			}
		}

		/*
		 * Declare services provided by the subsystem.
		 */
		if (runtime.has_sub_node("provides")) {
			xml.node("provides", [&] () {
				runtime.sub_node("provides").for_each_sub_node([&] (Xml_node service) {
					_gen_provides_sub_node(xml, service, "audio_in",    "Audio_in");
					_gen_provides_sub_node(xml, service, "audio_out",   "Audio_out");
					_gen_provides_sub_node(xml, service, "block",       "Block");
					_gen_provides_sub_node(xml, service, "file_system", "File_system");
					_gen_provides_sub_node(xml, service, "framebuffer", "Framebuffer");
					_gen_provides_sub_node(xml, service, "input",       "Input");
					_gen_provides_sub_node(xml, service, "log",         "LOG");
					_gen_provides_sub_node(xml, service, "nic",         "Nic");
					_gen_provides_sub_node(xml, service, "nitpicker",   "Nitpicker");
					_gen_provides_sub_node(xml, service, "report",      "Report");
					_gen_provides_sub_node(xml, service, "rom",         "ROM");
					_gen_provides_sub_node(xml, service, "terminal",    "Terminal");
					_gen_provides_sub_node(xml, service, "timer",       "Timer");
				});
			});
		}

		xml.node("route", [&] () {
			_gen_routes(xml, common, cached_depot_rom, uncached_depot_rom); });
	});
}


void Depot_deploy::Child::_gen_routes(Xml_generator &xml, Xml_node common,
                                      Depot_rom_server const &cached_depot_rom,
                                      Depot_rom_server const &uncached_depot_rom) const
{
	if (!_pkg_xml.constructed())
		return;

	typedef String<160> Path;

	/*
	 * Add routes given in the start node.
	 */
	if (_start_xml->xml().has_sub_node("route")) {
		Xml_node const route = _start_xml->xml().sub_node("route");
		route.with_raw_content([&] (char const *start, size_t length) {
			xml.append(start, length); });
	}

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
			xml.node("service", [&] () {
				xml.attribute("name",  "ROM");
				xml.attribute("label", "config");
				typedef String<160> Path;
				Path const path = rom.attribute_value("path", Path());

				if (cached_depot_rom.valid())
					xml.node("child", [&] () {
						xml.attribute("name", rom_server(path));
						xml.attribute("label", path); });
				else
					xml.node("parent", [&] () {
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

		xml.node("service", [&] () {
			xml.attribute("name", "ROM");
			xml.attribute("label_last", label);

			if (cached_depot_rom.valid()) {
				xml.node("child", [&] () {
					xml.attribute("name",  rom_server(path));
					xml.attribute("label", path);
				});
			} else {
				xml.node("parent", [&] () {
					xml.attribute("label", path); });
			}
		});
	});
}

#endif /* _CHILD_H_ */
