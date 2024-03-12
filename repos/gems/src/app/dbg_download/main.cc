/*
 * \brief  Component which initiates the download of missing 'bin' and 'dbg'
 *         depot archives based on the current runtime/monitor configuration
 * \author Christian Prochaska
 * \date   2023-12-04
 */

/*
 * Copyright (C) 2023-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <os/reporter.h>
#include <os/vfs.h>

using namespace Genode;

struct Main
{
	Genode::Env &_env;
	Heap         _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config     { _env, "config" };
	Attached_rom_dataspace _build_info { _env, "build_info" };

	Session_label _base_archive { };

	Root_directory _root_dir { _env, _heap, _config.xml().sub_node("vfs") };

	static constexpr char const *_runtime_config_path { "/config/managed/runtime" };

	Watch_handler<Main> _runtime_config_watch_handler {
		_env.ep(), _root_dir, _runtime_config_path,
		*this, &Main::_handle_runtime_config_update };

	Expanding_reporter _installation { _env, "installation", "installation" };


	void _process_monitor_config(Xml_node const &config, Xml_node const &monitor)
	{
		_installation.generate([&] (Xml_generator &xml) {

			monitor.for_each_sub_node("policy", [&] (Xml_node const &policy) {

				Session_label policy_label {
					policy.attribute_value("label", Session_label::String()) };

				config.for_each_sub_node("start", [&] (Xml_node const &start) {

					if (start.attribute_value("name", Session_label::String()) !=
					    policy_label)
						return;

					start.with_sub_node("route", [&] (Xml_node const &route) {

						route.for_each_sub_node("service", [&] (Xml_node const &service) {

							if (service.attribute_value("name", String<8>()) != "ROM")
								return;

							if (!service.has_attribute("label_last"))
								return;

							Session_label rom_session_label;

							if (service.attribute_value("label_last", Session_label::String()) ==
							    "ld.lib.so") {

								rom_session_label = { _base_archive, "/ld.lib.so" };

							} else {

								service.with_sub_node("child", [&] (Xml_node const &child) {

									if (child.attribute_value("name", String<16>()) !=
									    "depot_rom")
										return;

									rom_session_label =
										child.attribute_value("label", Session_label::String());

								}, [&] () {
									Genode::warning("<child> XML node not found");
									return;
								});
							}

							Vfs::Absolute_path rom_session_label_path { rom_session_label };

							Vfs::Absolute_path depot_user_path { rom_session_label_path };
							while (!depot_user_path.has_single_element())
								depot_user_path.strip_last_element();

							Vfs::Absolute_path depot_type_path { rom_session_label_path };
							depot_type_path.strip_prefix(depot_user_path.string());
							while (!depot_type_path.has_single_element())
								depot_type_path.strip_last_element();
							if (depot_type_path != "/bin")
								return;

							Vfs::Absolute_path depot_component_path { rom_session_label_path };
							depot_component_path.strip_prefix(
								Directory::join(depot_user_path, depot_type_path).string());

							Vfs::Absolute_path bin_installation_path { depot_user_path };
							bin_installation_path.append("/bin");
							bin_installation_path.append(depot_component_path.string());
							bin_installation_path.strip_last_element();

							xml.node("archive", [&] () {
								xml.attribute("path", &bin_installation_path.string()[1]);
								xml.attribute("source", "no");
							});

							Vfs::Absolute_path dbg_installation_path { depot_user_path };
							dbg_installation_path.append("/dbg");
							dbg_installation_path.append(depot_component_path.string());
							dbg_installation_path.strip_last_element();

							xml.node("archive", [&] () {
								xml.attribute("path", &dbg_installation_path.string()[1]);
								xml.attribute("source", "no");
							});
						});
					}, [&] () {
						Genode::error("<route> XML node not found");
					});
				});
			});
		});
	}


	void _handle_runtime_config_update()
	{
		try {
			File_content const runtime_config {
				_heap, _root_dir, _runtime_config_path,
				File_content::Limit(512*1024)
			};

			runtime_config.xml([&] (Xml_node const config) {
				config.with_sub_node("monitor", [&] (Xml_node const &monitor) {
					_process_monitor_config(config, monitor);
				}, [&] () {
					Genode::error("<monitor> XML node not found");
				});
			});
		} catch (File_content::Truncated_during_read) {
			Genode::error("Could not read ", _runtime_config_path);
		}
	}


	Main(Genode::Env &env)
	: _env(env)
	{
		_base_archive = _build_info.xml().attribute_value("base",
		                                                  Session_label::String());
		_handle_runtime_config_update();
	}
};


void Component::construct(Genode::Env &env)
{
	static Main main(env);
}
