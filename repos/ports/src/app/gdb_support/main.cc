/*
 * \brief  Component which creates symlinks to depot binaries and
 *         debug info files based on the current runtime/monitor
 *         configuration
 * \author Christian Prochaska
 * \date   2023-10-13
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

	Directory _debug_dir { _root_dir, "/debug" };

	Watch_handler<Main> _runtime_config_watch_handler {
		_env.ep(), _root_dir, _runtime_config_path,
		*this, &Main::_handle_runtime_config_update };


	void _remove_outdated_debug_directories(Xml_node const &monitor)
	{
		bool dir_removed = false;

		do {

			dir_removed = false;

			_debug_dir.for_each_entry([&] (Directory::Entry &component_entry) {

				/*
				 * 'for_each_entry()' must be restarted when a
				 * directory has been removed from within.
				 */
				if (dir_removed)
					return;

				bool found_in_config = false;

				monitor.for_each_sub_node("policy", [&] (Xml_node const &policy) {
					Session_label policy_label {
						policy.attribute_value("label",
								               Session_label::String()) };
					if (policy_label == component_entry.name())
						found_in_config = true;
				});

				if (!found_in_config) {
					Directory component_dir { _debug_dir, component_entry.name() };
					component_dir.for_each_entry([&] (Directory::Entry &file_entry) {
						component_dir.unlink(file_entry.name()); });

					_debug_dir.unlink(component_entry.name());
					dir_removed = true;
				}
			});
		} while (dir_removed);
	}


	void _process_monitor_config(Xml_node const &config, Xml_node const &monitor)
	{
		monitor.for_each_sub_node("policy", [&] (Xml_node const &policy) {

			Session_label policy_label {
				policy.attribute_value("label", Session_label::String()) };

			if (_debug_dir.directory_exists(policy_label))
				return;

			_debug_dir.create_sub_directory(policy_label);

			Directory component_dir { _debug_dir, policy_label };

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

						/* create symlink to binary */

						Vfs::Absolute_path depot_bin_path { "/depot" };
						depot_bin_path.append(depot_user_path.string());
						depot_bin_path.append("/bin");
						depot_bin_path.append(depot_component_path.string());
						Vfs::Absolute_path bin_file_path { depot_component_path };
						bin_file_path.keep_only_last_element();

						component_dir.create_symlink(bin_file_path, depot_bin_path);

						/* create symlink to debug info file */

						Vfs::Absolute_path depot_dbg_path { "/depot" };
						depot_dbg_path.append(depot_user_path.string());
						depot_dbg_path.append("/dbg");
						depot_dbg_path.append(depot_component_path.string());
						depot_dbg_path.append(".debug");
						Vfs::Absolute_path debug_file_path { depot_dbg_path };
						debug_file_path.keep_only_last_element();

						component_dir.create_symlink(debug_file_path, depot_dbg_path);
					});
				}, [&] () {
					Genode::error("<route> XML node not found");
				});
			});
		});

		_remove_outdated_debug_directories(monitor);
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
