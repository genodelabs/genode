/*
 * \brief  Sculpt system manager
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <os/path.h>
#include <gui_session/connection.h>
#include <vm_session/vm_session.h>
#include <timer_session/connection.h>
#include <io_port_session/io_port_session.h>
#include <event_session/event_session.h>
#include <capture_session/capture_session.h>
#include <gpu_session/gpu_session.h>
#include <pin_state_session/pin_state_session.h>
#include <pin_control_session/pin_control_session.h>

/* included from depot_deploy tool */
#include <children.h>

/* local includes */
#include <model/runtime_state.h>
#include <model/child_exit_state.h>
#include <model/file_operation_queue.h>
#include <model/settings.h>
#include <view/download_status.h>
#include <view/popup_dialog.h>
#include <view/panel_dialog.h>
#include <view/settings_dialog.h>
#include <view/file_browser_dialog.h>
#include <menu_view.h>
#include <gui.h>
#include <keyboard_focus.h>
#include <network.h>
#include <storage.h>
#include <deploy.h>
#include <graph.h>

namespace Sculpt { struct Main; }


struct Sculpt::Main : Input_event_handler,
                      Dialog::Generator,
                      Runtime_config_generator,
                      Storage::Target_user,
                      Network::Action,
                      Graph::Action,
                      Panel_dialog::Action,
                      Popup_dialog::Action,
                      Settings_dialog::Action,
                      File_browser_dialog::Action,
                      Popup_dialog::Construction_info,
                      Depot_query,
                      Panel_dialog::State,
                      Dialog,
                      Popup_dialog::Refresh,
                      Menu_view::Hover_update_handler
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Sculpt_version const _sculpt_version { _env };

	Registry<Child_state> _child_states { };

	Input::Seq_number _global_input_seq_number { };

	Gui::Connection _gui { _env, "input" };

	bool _gui_mode_ready = false;  /* becomes true once the graphics driver is up */

	Gui::Root _gui_root { _env, _heap, *this, _global_input_seq_number };

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	void _handle_input()
	{
		_gui.input()->for_each_event([&] (Input::Event const &ev) {
			handle_input_event(ev); });
	}

	Managed_config<Main> _system_config {
		_env, "system", "system", *this, &Main::_handle_system_config };

	void _handle_system_config(Xml_node)
	{
		_system_config.try_generate_manually_managed();
	}

	Signal_handler<Main> _gui_mode_handler {
		_env.ep(), *this, &Main::_handle_gui_mode };

	void _handle_gui_mode();

	Managed_config<Main> _fonts_config {
		_env, "config", "fonts", *this, &Main::_handle_fonts_config };

	void _handle_fonts_config(Xml_node config)
	{
		/*
		 * Obtain font size from manually maintained fonts configuration
		 * so that we can adjust the GUI layout accordingly.
		 */
		config.for_each_sub_node("vfs", [&] (Xml_node vfs) {
			vfs.for_each_sub_node("dir", [&] (Xml_node dir) {
				if (dir.attribute_value("name", String<16>()) == "fonts") {
					dir.for_each_sub_node("dir", [&] (Xml_node type) {
						if (type.attribute_value("name", String<16>()) == "text") {
							type.for_each_sub_node("ttf", [&] (Xml_node ttf) {
								double const px = ttf.attribute_value("size_px", 0.0);
								if (px > 0.0)
									_font_size_px = px; }); } }); } }); });

		_font_size_px = max(_font_size_px, _min_font_size_px);

		_handle_gui_mode();

		/* visibility of fonts section of settings dialog may have changed */
		_settings_menu_view.generate();

		/* visibility of settings button may have changed */
		_refresh_panel_and_window_layout();
	}

	Managed_config<Main> _event_filter_config {
		_env, "config", "event_filter", *this, &Main::_handle_event_filter_config };

	void _generate_event_filter_config(Xml_generator &);

	void _handle_event_filter_config(Xml_node)
	{
		_update_event_filter_config();
	}

	void _update_event_filter_config()
	{
		bool const orig_settings_available = _settings.interactive_settings_available();

		_settings.manual_event_filter_config =
			_event_filter_config.try_generate_manually_managed();

		if (!_settings.manual_event_filter_config)
			_event_filter_config.generate([&] (Xml_generator &xml) {
				_generate_event_filter_config(xml); });

		_settings_menu_view.generate();

		/* visibility of the settings dialog may have changed */
		if (orig_settings_available != _settings.interactive_settings_available()) {
			_refresh_panel_and_window_layout();
			_handle_gui_mode();
		}
	}


	/**********************
	 ** Device discovery **
	 **********************/

	Attached_rom_dataspace _pci_devices { _env, "report -> drivers/pci_devices" };

	Signal_handler<Main> _pci_devices_handler {
		_env.ep(), *this, &Main::_handle_pci_devices };

	Pci_info _pci_info { };

	void _handle_pci_devices()
	{
		_pci_devices.update();
		_pci_info.wifi_present  = false;
		_pci_info.lan_present   = true;
		_pci_info.modem_present = false;

		_pci_devices.xml().for_each_sub_node("device", [&] (Xml_node device) {
			device.with_optional_sub_node("pci-config", [&] (Xml_node pci) {
				/* detect Intel Wireless card */
				if (pci.attribute_value("class", 0UL) == 0x28000)
					_pci_info.wifi_present = true;
			});
		});

		update_network_dialog();
	}


	/***************************
	 ** Configuration loading **
	 ***************************/

	Prepare_version _prepare_version   { 0 };
	Prepare_version _prepare_completed { 0 };

	bool _prepare_in_progress() const
	{
		return _prepare_version.value != _prepare_completed.value;
	}


	Storage _storage { _env, _heap, _child_states, *this, *this, *this };

	/**
	 * Storage::Target_user interface
	 */
	void use_storage_target(Storage_target const &target) override
	{
		_storage._sculpt_partition = target;

		/* trigger loading of the configuration from the sculpt partition */
		_prepare_version.value++;

		_deploy.restart();

		generate_runtime_config();
	}

	Network _network { _env, _heap, *this, _child_states, *this, _runtime_state, _pci_info };

	Menu_view _network_menu_view { _env, _child_states, _network.dialog, "network_view",
	                               Ram_quota{4*1024*1024}, Cap_quota{150},
	                               "network_dialog", "network_view_hover",
	                               *this };

	/**
	 * Network::Action interface
	 */
	void update_network_dialog() override
	{
		_network_menu_view.generate();
	}


	/************
	 ** Update **
	 ************/

	Attached_rom_dataspace _update_state_rom {
		_env, "report -> runtime/update/state" };

	void _handle_update_state();

	Signal_handler<Main> _update_state_handler {
		_env.ep(), *this, &Main::_handle_update_state };

	/**
	 * Condition for spawning the update subsystem
	 */
	bool _update_running() const
	{
		return _storage._sculpt_partition.valid()
		    && !_prepare_in_progress()
		    && _network.ready()
		    && _deploy.update_needed();
	}

	Download_queue _download_queue { _heap };

	File_operation_queue _file_operation_queue { _heap };

	Fs_tool_version _fs_tool_version { 0 };


	/*****************
	 ** Depot query **
	 *****************/

	Depot_query::Version _query_version { 0 };

	Expanding_reporter _depot_query_reporter { _env, "query", "depot_query"};

	/**
	 * Depot_query interface
	 */
	Depot_query::Version depot_query_version() const override
	{
		return _query_version;
	}

	Timer::Connection _timer { _env };

	Timer::One_shot_timeout<Main> _deferred_depot_query_handler {
		_timer, *this, &Main::_handle_deferred_depot_query };

	void _handle_deferred_depot_query(Duration)
	{
		if (_deploy._arch.valid()) {
			_query_version.value++;
			_depot_query_reporter.generate([&] (Xml_generator &xml) {
				xml.attribute("arch",    _deploy._arch);
				xml.attribute("version", _query_version.value);

				_popup_dialog.gen_depot_query(xml);

				/* update query for blueprints of all unconfigured start nodes */
				_deploy.gen_depot_query(xml);
			});
		}
	}

	/**
	 * Depot_query interface
	 */
	void trigger_depot_query() override
	{
		/*
		 * Defer the submission of the query for a few milliseconds because
		 * 'trigger_depot_query' may be consecutively called several times
		 * while evaluating different conditions. Without deferring, the depot
		 * query component would produce intermediate results that take time
		 * but are ultimately discarded.
		 */
		_deferred_depot_query_handler.schedule(Microseconds{5000});
	}


	/*********************
	 ** Blueprint query **
	 *********************/

	Attached_rom_dataspace _blueprint_rom { _env, "report -> runtime/depot_query/blueprint" };

	Signal_handler<Main> _blueprint_handler {
		_env.ep(), *this, &Main::_handle_blueprint };

	void _handle_blueprint()
	{
		_blueprint_rom.update();

		Xml_node const blueprint = _blueprint_rom.xml();

		/*
		 * Drop intermediate results that will be superseded by a newer query.
		 * This is important because an outdated blueprint would be disregarded
		 * by 'handle_deploy' anyway while at the same time a new query is
		 * issued. This can result a feedback loop where blueprints are
		 * requested but never applied.
		 */
		if (blueprint.attribute_value("version", 0U) != _query_version.value)
			return;

		_runtime_state.apply_to_construction([&] (Component &component) {
			_popup_dialog.apply_blueprint(component, blueprint); });

		_deploy.handle_deploy();
	}


	/************
	 ** Deploy **
	 ************/

	Deploy::Prio_levels const _prio_levels { 4 };

	Attached_rom_dataspace _launcher_listing_rom {
		_env, "report -> /runtime/launcher_query/listing" };

	Launchers _launchers { _heap };

	Signal_handler<Main> _launcher_listing_handler {
		_env.ep(), *this, &Main::_handle_launcher_listing };

	void _handle_launcher_listing()
	{
		_launcher_listing_rom.update();

		Xml_node listing = _launcher_listing_rom.xml();
		if (listing.has_sub_node("dir")) {
			Xml_node dir = listing.sub_node("dir");

			/* let 'update_from_xml' iterate over <file> nodes */
			_launchers.update_from_xml(dir);
		}

		_popup_menu_view.generate();
		_deploy._handle_managed_deploy();
	}

	Deploy _deploy { _env, _heap, _child_states, _runtime_state, *this, *this, *this,
	                 _launcher_listing_rom, _blueprint_rom, _download_queue };

	Attached_rom_dataspace _manual_deploy_rom { _env, "config -> deploy" };

	void _handle_manual_deploy()
	{
		_runtime_state.reset_abandoned_and_launched_children();
		_manual_deploy_rom.update();
		_deploy.update_managed_deploy_config(_manual_deploy_rom.xml());
	}

	Signal_handler<Main> _manual_deploy_handler {
		_env.ep(), *this, &Main::_handle_manual_deploy };


	/************
	 ** Global **
	 ************/

	Settings _settings { };

	double const _min_font_size_px = 6.0;

	double _font_size_px = 14;

	Area _screen_size { };

	Panel_dialog::Tab _selected_tab = Panel_dialog::Tab::COMPONENTS;

	bool _log_visible      = false;
	bool _network_visible  = false;
	bool _settings_visible = false;

	File_browser_state _file_browser_state { };

	Attached_rom_dataspace _editor_saved_rom { _env, "report -> runtime/editor/saved" };

	Affinity::Space _affinity_space { 1, 1 };

	/**
	 * Panel_dialog::State interface
	 */
	bool log_visible() const override { return _log_visible; }

	bool network_visible() const override { return _network_visible; }

	bool settings_visible() const override { return _settings_visible; }

	bool inspect_tab_visible() const override { return _storage.any_file_system_inspected(); }

	Panel_dialog::Tab selected_tab() const override { return _selected_tab; }

	bool settings_available() const override { return _settings.interactive_settings_available(); }

	/**
	 * Dialog interface
	 */
	Hover_result hover(Xml_node) override;

	void reset() override { }

	/**
	 * Dialog interface
	 */
	void generate(Xml_generator &xml) const override
	{
		xml.node("vbox", [&] () {
			if (_manually_managed_runtime)
				return;

			bool const network_missing = _deploy.update_needed()
			                         && !_network._nic_state.ready();
			bool const show_diagnostics =
				_deploy.any_unsatisfied_child() || network_missing;

			auto gen_network_diagnostics = [&] (Xml_generator &xml)
			{
				if (!network_missing)
					return;

				gen_named_node(xml, "hbox", "network", [&] () {
					gen_named_node(xml, "float", "left", [&] () {
						xml.attribute("west", "yes");
						xml.node("label", [&] () {
							xml.attribute("text", "network needed for installation");
							xml.attribute("font", "annotation/regular");
						});
					});
				});
			};

			if (show_diagnostics) {
				gen_named_node(xml, "frame", "diagnostics", [&] () {
					xml.node("vbox", [&] () {

						xml.node("label", [&] () {
							xml.attribute("text", "Diagnostics"); });

						xml.node("float", [&] () {
							xml.node("vbox", [&] () {
								gen_network_diagnostics(xml);
								_deploy.gen_child_diagnostics(xml);
							});
						});
					});
				});
			}

			Xml_node const state = _update_state_rom.xml();
			if (_update_running() && state.attribute_value("progress", false))
				gen_download_status(xml, state);
		});
	}

	/**
	 * Dialog::Generator interface
	 */
	void generate_dialog() override
	{
		_main_menu_view.generate();
		_graph_menu_view.generate();
	}

	Attached_rom_dataspace _runtime_state_rom { _env, "report -> runtime/state" };

	Runtime_state _runtime_state { _heap, _storage._sculpt_partition };

	Managed_config<Main> _runtime_config {
		_env, "config", "runtime", *this, &Main::_handle_runtime };

	bool _manually_managed_runtime = false;

	void _handle_runtime(Xml_node config)
	{
		_manually_managed_runtime = !config.has_type("empty");
		generate_runtime_config();
		generate_dialog();
	}

	void _generate_runtime_config(Xml_generator &) const;

	/**
	 * Runtime_config_generator interface
	 */
	void generate_runtime_config() override
	{
		if (!_runtime_config.try_generate_manually_managed())
			_runtime_config.generate([&] (Xml_generator &xml) {
				_generate_runtime_config(xml); });
	}

	Signal_handler<Main> _runtime_state_handler {
		_env.ep(), *this, &Main::_handle_runtime_state };

	void _handle_runtime_state();

	Attached_rom_dataspace const _platform { _env, "platform_info" };


	/****************************************
	 ** Cached model of the runtime config **
	 ****************************************/

	/*
	 * Even though the runtime configuration is generated by the sculpt
	 * manager, we still obtain it as a separate ROM session to keep the GUI
	 * part decoupled from the lower-level runtime configuration generator.
	 */
	Attached_rom_dataspace _runtime_config_rom { _env, "config -> managed/runtime" };

	Signal_handler<Main> _runtime_config_handler {
		_env.ep(), *this, &Main::_handle_runtime_config };

	Runtime_config _cached_runtime_config { _heap };

	void _handle_runtime_config()
	{
		_runtime_config_rom.update();
		_cached_runtime_config.update_from_xml(_runtime_config_rom.xml());
		_graph_menu_view.generate();

		if (_selected_tab == Panel_dialog::Tab::FILES)
			_file_browser_menu_view.generate();
	}


	/****************************
	 ** Interactive operations **
	 ****************************/

	/*
	 * Track nitpicker's "clicked" report to reliably detect clicks outside
	 * any menu view (closing the popup window).
	 */

	Attached_rom_dataspace _clicked_rom { _env, "clicked" };

	Signal_handler<Main> _clicked_handler {
		_env.ep(), *this, &Main::_handle_clicked };

	void _handle_clicked()
	{
		_clicked_rom.update();
		_try_handle_click();
	}

	Keyboard_focus _keyboard_focus { _env, _network.dialog, _network.wpa_passphrase, *this };

	Constructible<Input::Seq_number> _clicked_seq_number { };
	Constructible<Input::Seq_number> _clacked_seq_number { };

	void _try_handle_click()
	{
		if (!_clicked_seq_number.constructed())
			return;

		Input::Seq_number const seq = *_clicked_seq_number;

		auto click_outside_popup = [&] ()
		{
			Xml_node const clicked = _clicked_rom.xml();

			if (!clicked.has_attribute("seq"))
				return false;

			if (clicked.attribute_value("seq", 0u) != seq.value)
				return false;

			Label const popup_label { "wm -> runtime -> leitzentrale -> popup_view" };

			if (clicked.attribute_value("label", Label()) == popup_label)
				return false;

			return true;
		};

		/* remove popup dialog when clicking somewhere outside */
		if (click_outside_popup() && _popup.state == Popup::VISIBLE
		 && !_graph.add_button_hovered()) {

			_popup.state = Popup::OFF;
			_popup_dialog.reset();
			discard_construction();

			/* de-select '+' button */
			_graph_menu_view.generate();

			/* remove popup window from window layout */
			_handle_window_layout();
		}

		if (_main_menu_view.hovered(seq)) {
			_main_menu_view.generate();
			_clicked_seq_number.destruct();
		}
		else if (_graph_menu_view.hovered(seq)) {
			_graph.click(*this);
			_graph_menu_view.generate();
			_clicked_seq_number.destruct();
		}
		else if (_popup_menu_view.hovered(seq)) {
			_popup_dialog.click(*this);
			_popup_menu_view.generate();
			_clicked_seq_number.destruct();
		}
		else if (_panel_menu_view.hovered(seq)) {
			_panel_dialog.click(*this);
			_clicked_seq_number.destruct();
		}
		else if (_settings_menu_view.hovered(seq)) {
			_settings_dialog.click(*this);
			_settings_menu_view.generate();
			_clicked_seq_number.destruct();
		}
		else if (_network_menu_view.hovered(seq)) {
			_network.dialog.click(_network);
			_network_menu_view.generate();
			_clicked_seq_number.destruct();
		}
		else if (_file_browser_menu_view.hovered(seq)) {
			_file_browser_dialog.click(*this);
			_file_browser_menu_view.generate();
			_clicked_seq_number.destruct();
		}
	}

	void _try_handle_clack()
	{
		if (!_clacked_seq_number.constructed())
			return;

		Input::Seq_number const seq = *_clacked_seq_number;

		if (_main_menu_view.hovered(seq)) {
			_storage.dialog.clack(_storage);
			_main_menu_view.generate();
			_clacked_seq_number.destruct();
		}
		else if (_graph_menu_view.hovered(seq)) {
			_graph.clack(*this, _storage);
			_graph_menu_view.generate();
			_clacked_seq_number.destruct();
		}
		else if (_popup_menu_view.hovered(seq)) {
			_popup_dialog.clack(*this);
			_clacked_seq_number.destruct();
		}
	}

	/**
	 * Menu_view::Hover_update_handler interface
	 */
	void menu_view_hover_updated() override
	{
		if (_clicked_seq_number.constructed())
			_try_handle_click();

		if (_clacked_seq_number.constructed())
			_try_handle_clack();
	}

	/**
	 * Input_event_handler interface
	 */
	void handle_input_event(Input::Event const &ev) override
	{
		bool need_generate_dialog = false;

		if (ev.key_press(Input::BTN_LEFT) || ev.touch()) {
			_clicked_seq_number.construct(_global_input_seq_number);
			_try_handle_click();
		}

		if (ev.key_release(Input::BTN_LEFT)) {
			_clacked_seq_number.construct(_global_input_seq_number);
			_try_handle_clack();
		}

		if (_keyboard_focus.target == Keyboard_focus::WPA_PASSPHRASE)
			ev.handle_press([&] (Input::Keycode, Codepoint code) {
				_network.handle_key_press(code); });

		if (ev.press())
			_keyboard_focus.update();

		if (need_generate_dialog)
			generate_dialog();
	}

	/*
	 * Fs_dialog::Action interface
	 */
	void toggle_inspect_view(Storage_target const &target) override
	{
		_storage.toggle_inspect_view(target);

		/* refresh visibility to inspect tab */
		_panel_menu_view.generate();
	}

	void use(Storage_target const &target) override { _storage.use(target); }

	/*
	 * Storage_dialog::Action interface
	 */
	void format(Storage_target const &target) override
	{
		_storage.format(target);
	}

	void cancel_format(Storage_target const &target) override
	{
		_storage.cancel_format(target);
		_graph.reset_storage_operation();
	}

	void expand(Storage_target const &target) override
	{
		_storage.expand(target);
	}

	void cancel_expand(Storage_target const &target) override
	{
		_storage.cancel_expand(target);
		_graph.reset_storage_operation();
	}

	void check(Storage_target const &target) override
	{
		_storage.check(target);
	}

	void toggle_default_storage_target(Storage_target const &target) override
	{
		_storage.toggle_default_storage_target(target);
	}

	/*
	 * Graph::Action interface
	 */
	void remove_deployed_component(Start_name const &name) override
	{
		_runtime_state.abandon(name);

		/* update config/managed/deploy with the component 'name' removed */
		_deploy.update_managed_deploy_config(_manual_deploy_rom.xml());
	}

	/*
	 * Graph::Action interface
	 */
	void restart_deployed_component(Start_name const &name) override
	{
		if (name == "nic_drv") {

			_network.restart_nic_drv_on_next_runtime_cfg();
			generate_runtime_config();

		} else if (name == "wifi_drv") {

			_network.restart_wifi_drv_on_next_runtime_cfg();
			generate_runtime_config();

		} else if (name == "usb_net") {

			_network.restart_usb_net_on_next_runtime_cfg();
			generate_runtime_config();

		} else {

			_runtime_state.restart(name);

			/* update config/managed/deploy with the component 'name' removed */
			_deploy.update_managed_deploy_config(_manual_deploy_rom.xml());
		}
	}

	/*
	 * Graph::Action interface
	 */
	void toggle_launcher_selector(Rect anchor) override
	{
		_popup_menu_view.generate();
		_popup.anchor = anchor;
		_popup.toggle();
		_graph_menu_view.generate();
		_handle_window_layout();
	}

	void _refresh_panel_and_window_layout()
	{
		_panel_menu_view.generate();
		_handle_window_layout();
	}

	/*
	 * Panel::Action interface
	 */
	void select_tab(Panel_dialog::Tab tab) override
	{
		_selected_tab = tab;

		if (_selected_tab == Panel_dialog::Tab::FILES)
			_file_browser_menu_view.generate();

		_refresh_panel_and_window_layout();
	}

	/*
	 * Panel::Action interface
	 */
	void toggle_log_visibility() override
	{
		_log_visible = !_log_visible;
		_refresh_panel_and_window_layout();
	}

	/*
	 * Panel::Action interface
	 */
	void toggle_network_visibility() override
	{
		_network_visible = !_network_visible;
		_refresh_panel_and_window_layout();
	}

	/*
	 * Panel::Action interface
	 */
	void toggle_settings_visibility() override
	{
		_settings_visible = !_settings_visible;
		_refresh_panel_and_window_layout();
	}

	/*
	 * Settings_dialog::Action interface
	 */
	void select_font_size(Settings::Font_size font_size) override
	{
		if (_settings.font_size == font_size)
			return;

		_settings.font_size = font_size;
		_handle_gui_mode();
	}

	/*
	 * Settings_dialog::Action interface
	 */
	void select_keyboard_layout(Settings::Keyboard_layout::Name const &keyboard_layout) override
	{
		if (_settings.keyboard_layout == keyboard_layout)
			return;

		_settings.keyboard_layout = keyboard_layout;

		_update_event_filter_config();
	}

	Signal_handler<Main> _fs_query_result_handler {
		_env.ep(), *this, &Main::_handle_fs_query_result };

	void _handle_fs_query_result()
	{
		_file_browser_state.update_query_results();
		_file_browser_menu_view.generate();
	}

	Signal_handler<Main> _editor_saved_handler {
		_env.ep(), *this, &Main::_handle_editor_saved };

	void _handle_editor_saved()
	{
		_editor_saved_rom.update();

		Xml_node const saved = _editor_saved_rom.xml();

		bool const orig_modified = _file_browser_state.modified;

		_file_browser_state.modified           = saved.attribute_value("modified", false);
		_file_browser_state.last_saved_version = saved.attribute_value("version", 0U);

		if (orig_modified != _file_browser_state.modified)
			_file_browser_menu_view.generate();
	}

	void _close_edited_file()
	{
		_file_browser_state.edited_file = File_browser_state::File();
		_file_browser_state.text_area.destruct();
		_file_browser_state.edit = false;
	}

	/*
	 * File_browser_dialog::Action interface
	 */
	void browse_file_system(File_browser_state::Fs_name const &name) override
	{
		using Fs_name = File_browser_state::Fs_name;

		_close_edited_file();

		if (name == _file_browser_state.browsed_fs) {
			_file_browser_state.browsed_fs = Fs_name();
			_file_browser_state.fs_query.destruct();

		} else {
			_file_browser_state.browsed_fs = name;
			_file_browser_state.path = File_browser_state::Path("/");

			Start_name const start_name(name, ".query");
			_file_browser_state.fs_query.construct(_child_states, start_name,
			                                       Priority::LEITZENTRALE,
			                                       Ram_quota{8*1024*1024}, Cap_quota{200});

			Label const rom_label("report -> /runtime/", start_name, "/listing");

			_file_browser_state.query_result.construct(_env, rom_label.string());
			_file_browser_state.query_result->sigh(_fs_query_result_handler);
			_handle_fs_query_result();
		}

		generate_runtime_config();

		_file_browser_menu_view.generate();
	}

	void browse_sub_directory(File_browser_state::Sub_dir const &sub_dir) override
	{
		_close_edited_file();

		if (_file_browser_state.path == "/")
			_file_browser_state.path =
				File_browser_state::Path("/", sub_dir);
		else
			_file_browser_state.path =
				File_browser_state::Path(_file_browser_state.path, "/", sub_dir);

		generate_runtime_config();
	}

	void browse_parent_directory() override
	{
		_close_edited_file();

		Genode::Path<256> path(_file_browser_state.path);
		path.strip_last_element();
		_file_browser_state.path = File_browser_state::Path(path);

		generate_runtime_config();
	}

	void browse_abs_directory(File_browser_state::Path const &path) override
	{
		_close_edited_file();

		_file_browser_state.path = path;

		generate_runtime_config();
	}

	void _view_or_edit_file(File_browser_state::File const &file, bool edit)
	{
		if (_file_browser_state.edited_file == file) {
			_close_edited_file();
		} else {
			_file_browser_state.edited_file  = file;
			_file_browser_state.edit         = edit;
			_file_browser_state.save_version = 0;

			if (_file_browser_state.text_area.constructed()) {
				_file_browser_state.text_area->trigger_restart();
			} else {
				Start_name const start_name("editor");
				_file_browser_state.text_area.construct(_child_states, start_name,
				                                        Priority::LEITZENTRALE,
				                                        Ram_quota{16*1024*1024}, Cap_quota{250});
			}
		}

		generate_runtime_config();
	}

	void view_file(File_browser_state::File const &file) override
	{
		_view_or_edit_file(file, false);
	}

	void edit_file(File_browser_state::File const &file) override
	{
		_view_or_edit_file(file, true);
	}

	void revert_edited_file() override
	{
		if (_file_browser_state.text_area.constructed())
			_file_browser_state.text_area->trigger_restart();

		generate_runtime_config();
	}

	void save_edited_file() override
	{
		_file_browser_state.save_version = _file_browser_state.last_saved_version + 1;
		generate_runtime_config();
	}

	void _close_popup_dialog()
	{
		/* close popup menu */
		_popup.state = Popup::OFF;
		_popup_dialog.reset();
		_handle_window_layout();

		/* reset state of the '+' button */
		_graph_menu_view.generate();
	}

	/*
	 * Popup_dialog::Action interface
	 */
	void launch_global(Path const &launcher) override
	{
		_runtime_state.launch(launcher, launcher);

		_close_popup_dialog();

		/* trigger change of the deployment */
		_deploy.update_managed_deploy_config(_manual_deploy_rom.xml());
	}

	Start_name new_construction(Component::Path const &pkg,
	                            Component::Info const &info) override
	{
		return _runtime_state.new_construction(pkg, info, _affinity_space);
	}

	void _apply_to_construction(Popup_dialog::Action::Apply_to &fn) override
	{
		_runtime_state.apply_to_construction([&] (Component &c) { fn.apply_to(c); });
	}

	void discard_construction() override { _runtime_state.discard_construction(); }

	void launch_construction()  override
	{
		_runtime_state.launch_construction();

		_close_popup_dialog();

		/* trigger change of the deployment */
		_deploy.update_managed_deploy_config(_manual_deploy_rom.xml());
	}

	void trigger_download(Path const &path) override
	{
		_download_queue.add(path);

		/* incorporate new download-queue content into update */
		_deploy.update_installation();

		generate_runtime_config();
	}

	void remove_index(Depot::Archive::User const &user) override
	{
		auto remove = [&] (Path const &path) {
			_file_operation_queue.remove_file(path); };

		remove(Path("/rw/depot/",  user, "/index/", _sculpt_version));
		remove(Path("/rw/public/", user, "/index/", _sculpt_version, ".xz"));
		remove(Path("/rw/public/", user, "/index/", _sculpt_version, ".xz.sig"));

		if (!_file_operation_queue.any_operation_in_progress())
			_file_operation_queue.schedule_next_operations();

		generate_runtime_config();
	}

	/**
	 * Popup_dialog::Construction_info interface
	 */
	void _with_construction(Popup_dialog::Construction_info::With const &fn) const override
	{
		_runtime_state.with_construction([&] (Component const &c) { fn.with(c); });
	}

	Panel_dialog _panel_dialog { *this };

	Menu_view _panel_menu_view { _env, _child_states, _panel_dialog, "panel_view",
	                             Ram_quota{4*1024*1024}, Cap_quota{150},
	                             "panel_dialog", "panel_view_hover", *this };

	Settings_dialog _settings_dialog { _settings };

	Menu_view _settings_menu_view { _env, _child_states, _settings_dialog, "settings_view",
	                                Ram_quota{4*1024*1024}, Cap_quota{150},
	                                "settings_dialog", "settings_view_hover", *this };

	Menu_view _main_menu_view { _env, _child_states, *this, "menu_view",
	                             Ram_quota{4*1024*1024}, Cap_quota{150},
	                             "menu_dialog", "menu_view_hover", *this };

	Popup_dialog _popup_dialog { _env, *this, _launchers,
	                             _network._nic_state, _network._nic_target,
	                             _runtime_state, _cached_runtime_config,
	                             _download_queue, *this, *this };

	Menu_view _popup_menu_view { _env, _child_states, _popup_dialog, "popup_view",
	                             Ram_quota{4*1024*1024}, Cap_quota{150},
	                             "popup_dialog", "popup_view_hover", *this };

	File_browser_dialog _file_browser_dialog { _cached_runtime_config, _file_browser_state };

	Menu_view _file_browser_menu_view { _env, _child_states, _file_browser_dialog, "file_browser_view",
	                                    Ram_quota{8*1024*1024}, Cap_quota{150},
	                                    "file_browser_dialog", "file_browser_view_hover", *this };

	/**
	 * Popup_dialog::Refresh interface
	 */
	void refresh_popup_dialog() override { _popup_menu_view.generate(); }

	Managed_config<Main> _fb_drv_config {
		_env, "config", "fb_drv", *this, &Main::_handle_fb_drv_config };

	void _handle_fb_drv_config(Xml_node)
	{
		_fb_drv_config.try_generate_manually_managed();
	}

	void _handle_window_layout();

	template <size_t N, typename FN>
	void _with_window(Xml_node window_list, String<N> const &match, FN const &fn)
	{
		window_list.for_each_sub_node("window", [&] (Xml_node win) {
			if (win.attribute_value("label", String<N>()) == match)
				fn(win); });
	}

	Attached_rom_dataspace _window_list { _env, "window_list" };

	Signal_handler<Main> _window_list_handler {
		_env.ep(), *this, &Main::_handle_window_layout };

	Expanding_reporter _wm_focus { _env, "focus", "wm_focus" };

	Attached_rom_dataspace _decorator_margins { _env, "decorator_margins" };

	Signal_handler<Main> _decorator_margins_handler {
		_env.ep(), *this, &Main::_handle_window_layout };

	Expanding_reporter _window_layout { _env, "window_layout", "window_layout" };


	/*******************
	 ** Runtime graph **
	 *******************/

	Popup _popup { };

	Graph _graph { _runtime_state, _cached_runtime_config, _storage._storage_devices,
	               _storage._sculpt_partition, _storage._ram_fs_state,
	               _popup.state, _deploy._children };

	Menu_view _graph_menu_view { _env, _child_states, _graph, "runtime_view",
	                             Ram_quota{8*1024*1024}, Cap_quota{200},
	                             "runtime_dialog", "runtime_view_hover", *this };
	Main(Env &env) : _env(env)
	{
		_manual_deploy_rom.sigh(_manual_deploy_handler);
		_runtime_state_rom.sigh(_runtime_state_handler);
		_runtime_config_rom.sigh(_runtime_config_handler);
		_gui.input()->sigh(_input_handler);
		_gui.mode_sigh(_gui_mode_handler);

		/*
		 * Subscribe to reports
		 */
		_update_state_rom    .sigh(_update_state_handler);
		_pci_devices         .sigh(_pci_devices_handler);
		_window_list         .sigh(_window_list_handler);
		_decorator_margins   .sigh(_decorator_margins_handler);
		_launcher_listing_rom.sigh(_launcher_listing_handler);
		_blueprint_rom       .sigh(_blueprint_handler);
		_editor_saved_rom    .sigh(_editor_saved_handler);
		_clicked_rom         .sigh(_clicked_handler);

		/*
		 * Generate initial configurations
		 */
		_network.wifi_disconnect();
		_update_event_filter_config();

		/*
		 * Import initial report content
		 */
		_handle_gui_mode();
		_storage.handle_storage_devices_update();
		_handle_pci_devices();
		_handle_runtime_config();
		_handle_clicked();

		/*
		 * Read static platform information
		 */
		_platform.xml().with_optional_sub_node("affinity-space", [&] (Xml_node const &node) {
			_affinity_space = Affinity::Space(node.attribute_value("width",  1U),
			                                  node.attribute_value("height", 1U));
		});

		/*
		 * Generate initial config/managed/deploy configuration
		 */
		_handle_manual_deploy();

		generate_runtime_config();
		generate_dialog();
	}
};


void Sculpt::Main::_handle_window_layout()
{
	/* skip window-layout handling (and decorator activity) while booting */
	if (!_gui_mode_ready)
		return;

	struct Decorator_margins
	{
		unsigned top = 0, bottom = 0, left = 0, right = 0;

		Decorator_margins(Xml_node node)
		{
			if (!node.has_sub_node("floating"))
				return;

			Xml_node const floating = node.sub_node("floating");

			top    = floating.attribute_value("top",    0U);
			bottom = floating.attribute_value("bottom", 0U);
			left   = floating.attribute_value("left",   0U);
			right  = floating.attribute_value("right",  0U);
		}
	};

	/* read decorator margins from the decorator's report */
	_decorator_margins.update();
	Decorator_margins const margins(_decorator_margins.xml());

	unsigned const log_min_w = 400;

	typedef String<128> Label;
	Label const
		inspect_label          ("runtime -> leitzentrale -> inspect"),
		runtime_view_label     ("runtime -> leitzentrale -> runtime_view"),
		panel_view_label       ("runtime -> leitzentrale -> panel_view"),
		menu_view_label        ("runtime -> leitzentrale -> menu_view"),
		popup_view_label       ("runtime -> leitzentrale -> popup_view"),
		settings_view_label    ("runtime -> leitzentrale -> settings_view"),
		network_view_label     ("runtime -> leitzentrale -> network_view"),
		file_browser_view_label("runtime -> leitzentrale -> file_browser_view"),
		editor_view_label      ("runtime -> leitzentrale -> editor"),
		logo_label             ("logo");

	_window_list.update();
	Xml_node const window_list = _window_list.xml();

	auto win_size = [&] (Xml_node win) { return Area::from_xml(win); };

	unsigned panel_height = 0;
	_with_window(window_list, panel_view_label, [&] (Xml_node win) {
		panel_height = win_size(win).h(); });

	/* suppress intermediate states during the restart of the panel */
	if (panel_height == 0)
		return;

	Framebuffer::Mode const mode = _gui.mode();

	/* suppress intermediate boot-time states before the framebuffer driver is up */
	if (mode.area.count() <= 1)
		return;

	/* area reserved for the panel */
	Rect const panel(Point(0, 0), Area(mode.area.w(), panel_height));

	/* available space on the right of the menu */
	Rect avail(Point(0, panel.h()),
	           Point(mode.area.w() - 1, mode.area.h() - 1));

	Point const log_offset = _log_visible
	                       ? Point(0, 0)
	                       : Point(log_min_w + margins.left + margins.right, 0);

	Point const log_p1(avail.x2() - log_min_w - margins.right + 1 + log_offset.x(),
	                   avail.y1() + margins.top);
	Point const log_p2(mode.area.w() - margins.right  - 1 + log_offset.x(),
	                   mode.area.h() - margins.bottom - 1);

	/* position of the inspect window */
	Point const inspect_p1(avail.x1() + margins.left, avail.y1() + margins.top);
	Point const inspect_p2(avail.x2() - margins.right - 1,
	                       avail.y2() - margins.bottom - 1);

	_window_layout.generate([&] (Xml_generator &xml) {

		auto gen_window = [&] (Xml_node win, Rect rect) {
			if (rect.valid()) {
				xml.node("window", [&] () {
					xml.attribute("id",     win.attribute_value("id", 0UL));
					xml.attribute("xpos",   rect.x1());
					xml.attribute("ypos",   rect.y1());
					xml.attribute("width",  rect.w());
					xml.attribute("height", rect.h());
					xml.attribute("title",  win.attribute_value("label", Label()));
				});
			}
		};

		/* window size limited to space unobstructed by the menu and log */
		auto constrained_win_size = [&] (Xml_node win) {

			unsigned const inspect_w = inspect_p2.x() - inspect_p1.x(),
			               inspect_h = inspect_p2.y() - inspect_p1.y();

			Area const size = win_size(win);
			return Area(min(inspect_w, size.w()), min(inspect_h, size.h()));
		};

		_with_window(window_list, panel_view_label, [&] (Xml_node win) {
			gen_window(win, panel); });

		_with_window(window_list, Label("log"), [&] (Xml_node win) {
			gen_window(win, Rect(log_p1, log_p2)); });

		_with_window(window_list, settings_view_label, [&] (Xml_node win) {
			Area  const size = win_size(win);
			Point const pos  = _settings_visible
			                 ? Point(0, avail.y1())
			                 : Point(-size.w(), avail.y1());

			if (_settings.interactive_settings_available())
				gen_window(win, Rect(pos, size));
		});

		_with_window(window_list, network_view_label, [&] (Xml_node win) {
			Area  const size = win_size(win);
			Point const pos  = _network_visible
			                 ? Point(log_p1.x() - size.w(), avail.y1())
			                 : Point(mode.area.w(), avail.y1());
			gen_window(win, Rect(pos, size));
		});

		_with_window(window_list, file_browser_view_label, [&] (Xml_node win) {
			if (_selected_tab == Panel_dialog::Tab::FILES) {

				Area  const size = constrained_win_size(win);
				Point const pos  = Rect(inspect_p1, inspect_p2).center(size);

				Point const offset = _file_browser_state.text_area.constructed()
				                   ? Point((2*avail.w())/3 - pos.x(), 0)
				                   : Point(0, 0);

				gen_window(win, Rect(pos - offset, size));
			}
		});

		_with_window(window_list, editor_view_label, [&] (Xml_node win) {
			if (_selected_tab == Panel_dialog::Tab::FILES) {
				Area  const size = constrained_win_size(win);
				Point const pos  = Rect(inspect_p1 + Point(400, 0), inspect_p2).center(size);

				Point const offset = _file_browser_state.text_area.constructed()
				                   ? Point(avail.w()/3 - pos.x(), 0)
				                   : Point(0, 0);

				gen_window(win, Rect(pos + offset, size));
			}
		});

		_with_window(window_list, menu_view_label, [&] (Xml_node win) {
			if (_selected_tab == Panel_dialog::Tab::COMPONENTS) {
				Area  const size = win_size(win);
				Point const pos(0, avail.y2() - size.h());
				gen_window(win, Rect(pos, size));
			}
		});

		/*
		 * Calculate centered runtime view within the available main (inspect)
		 * area.
		 */
		Point runtime_view_pos { };
		_with_window(window_list, runtime_view_label, [&] (Xml_node win) {
			Area const size  = constrained_win_size(win);
			runtime_view_pos = Rect(inspect_p1, inspect_p2).center(size);
		});

		if (_popup.state == Popup::VISIBLE) {
			_with_window(window_list, popup_view_label, [&] (Xml_node win) {
				Area const size = win_size(win);

				int const anchor_y_center = (_popup.anchor.y1() + _popup.anchor.y2())/2;

				int const x = runtime_view_pos.x() + _popup.anchor.x2();
				int const y = max((int)panel_height, runtime_view_pos.y() + anchor_y_center - (int)size.h()/2);

				gen_window(win, Rect(Point(x, y), size));
			});
		}

		_with_window(window_list, inspect_label, [&] (Xml_node win) {
			if (_selected_tab == Panel_dialog::Tab::INSPECT)
				gen_window(win, Rect(inspect_p1, inspect_p2)); });

		/*
		 * Position runtime view centered within the inspect area, but allow
		 * the overlapping of the log area. (use the menu view's 'win_size').
		 */
		_with_window(window_list, runtime_view_label, [&] (Xml_node win) {
			if (_selected_tab == Panel_dialog::Tab::COMPONENTS)
				gen_window(win, Rect(runtime_view_pos, win_size(win))); });

		_with_window(window_list, logo_label, [&] (Xml_node win) {
			Area  const size = win_size(win);
			Point const pos(mode.area.w() - size.w(), mode.area.h() - size.h());
			gen_window(win, Rect(pos, size));
		});
	});

	/* define window-manager focus */
	_wm_focus.generate([&] (Xml_generator &xml) {
		_window_list.xml().for_each_sub_node("window", [&] (Xml_node win) {
			Label const label = win.attribute_value("label", Label());

			if (label == inspect_label && _selected_tab == Panel_dialog::Tab::INSPECT)
				xml.node("window", [&] () {
					xml.attribute("id", win.attribute_value("id", 0UL)); });

			if (label == editor_view_label && _selected_tab == Panel_dialog::Tab::FILES)
				xml.node("window", [&] () {
					xml.attribute("id", win.attribute_value("id", 0UL)); });
		});
	});
}


void Sculpt::Main::_handle_gui_mode()
{
	Framebuffer::Mode const mode = _gui.mode();

	if (mode.area.count() > 1)
		_gui_mode_ready = true;

	_handle_window_layout();

	_settings.manual_fonts_config = _fonts_config.try_generate_manually_managed();

	if (!_settings.manual_fonts_config) {

		_font_size_px = (double)mode.area.h() / 60.0;

		if (_settings.font_size == Settings::Font_size::SMALL) _font_size_px *= 0.85;
		if (_settings.font_size == Settings::Font_size::LARGE) _font_size_px *= 1.35;

		/*
		 * Limit lower bound of font size. Otherwise, the glyph rendering
		 * may suffer from division-by-zero problems.
		 */
		_font_size_px = max(_font_size_px, _min_font_size_px);

		_fonts_config.generate([&] (Xml_generator &xml) {
			xml.attribute("copy",  true);
			xml.attribute("paste", true);
			xml.node("vfs", [&] () {
				gen_named_node(xml, "rom", "Vera.ttf");
				gen_named_node(xml, "rom", "VeraMono.ttf");
				gen_named_node(xml, "dir", "fonts", [&] () {

					auto gen_ttf_dir = [&] (char const *dir_name,
					                        char const *ttf_path, double size_px) {

						gen_named_node(xml, "dir", dir_name, [&] () {
							gen_named_node(xml, "ttf", "regular", [&] () {
								xml.attribute("path",    ttf_path);
								xml.attribute("size_px", size_px);
								xml.attribute("cache",   "256K");
							});
						});
					};

					gen_ttf_dir("title",      "/Vera.ttf",     _font_size_px*1.25);
					gen_ttf_dir("text",       "/Vera.ttf",     _font_size_px);
					gen_ttf_dir("annotation", "/Vera.ttf",     _font_size_px*0.8);
					gen_ttf_dir("monospace",  "/VeraMono.ttf", _font_size_px);
				});
			});
			xml.node("default-policy", [&] () { xml.attribute("root", "/fonts"); });

			auto gen_color = [&] (unsigned index, Color color) {
				xml.node("palette", [&] () {
					xml.node("color", [&] () {
						xml.attribute("index", index);
						xml.attribute("value", String<16>(color));
					});
				});
			};

			Color const background(0x1c, 0x22, 0x32);

			gen_color(0, background);
			gen_color(8, background);
		});
	}

	_screen_size = mode.area;
	_panel_menu_view.min_width = _screen_size.w();
	unsigned const menu_width = max((unsigned)(_font_size_px*21.0), 320u);
	_main_menu_view.min_width = menu_width;
	_network_menu_view.min_width = menu_width;

	/* font size may has changed, propagate fonts config of runtime view */
	generate_runtime_config();
}


Sculpt::Dialog::Hover_result Sculpt::Main::hover(Xml_node hover)
{
	return _storage.dialog.match_sub_dialog(hover, "vbox", "frame", "vbox");
}


void Sculpt::Main::_handle_update_state()
{
	_update_state_rom.update();
	generate_dialog();

	Xml_node const update_state = _update_state_rom.xml();

	if (update_state.num_sub_nodes() == 0)
		return;

	bool const popup_watches_downloads =
		_popup_dialog.interested_in_download();

	_download_queue.apply_update_state(update_state);
	_download_queue.remove_inactive_downloads();

	bool const installation_complete =
		!update_state.attribute_value("progress", false);

	if (installation_complete) {

		Xml_node const blueprint = _blueprint_rom.xml();
		bool const new_depot_query_needed = popup_watches_downloads
		                                 || blueprint_any_missing(blueprint)
		                                 || blueprint_any_rom_missing(blueprint);
		if (new_depot_query_needed)
			trigger_depot_query();

		if (popup_watches_downloads)
			_deploy.update_installation();

		_deploy.reattempt_after_installation();
	}
}


void Sculpt::Main::_handle_runtime_state()
{
	_runtime_state_rom.update();

	Xml_node state = _runtime_state_rom.xml();

	_runtime_state.update_from_state_report(state);

	bool reconfigure_runtime = false;
	bool regenerate_dialog   = false;

	/* check for completed storage operations */
	_storage._storage_devices.for_each([&] (Storage_device &device) {

		device.for_each_partition([&] (Partition &partition) {

			Storage_target const target { device.label, partition.number };

			if (partition.check_in_progress) {
				String<64> name(target.label(), ".e2fsck");
				Child_exit_state exit_state(state, name);

				if (exit_state.exited) {
					if (exit_state.code != 0)
						error("file-system check failed");
					if (exit_state.code == 0)
						log("file-system check succeeded");

					partition.check_in_progress = 0;
					reconfigure_runtime = true;
					_storage.dialog.reset_operation();
					_graph.reset_storage_operation();
				}
			}

			if (partition.format_in_progress) {
				String<64> name(target.label(), ".mke2fs");
				Child_exit_state exit_state(state, name);

				if (exit_state.exited) {
					if (exit_state.code != 0)
						error("file-system creation failed");

					partition.format_in_progress = false;
					partition.file_system.type = File_system::EXT2;

					if (partition.whole_device())
						device.rediscover();

					reconfigure_runtime = true;
					_storage.dialog.reset_operation();
					_graph.reset_storage_operation();
				}
			}

			/* respond to completion of file-system resize operation */
			if (partition.fs_resize_in_progress) {
				Child_exit_state exit_state(state, Start_name(target.label(), ".resize2fs"));
				if (exit_state.exited) {
					partition.fs_resize_in_progress = false;
					reconfigure_runtime = true;
					device.rediscover();
					_storage.dialog.reset_operation();
					_graph.reset_storage_operation();
				}
			}

		}); /* for each partition */

		/* respond to failure of part_block */
		if (device.discovery_in_progress()) {
			Child_exit_state exit_state(state, device.part_block_start_name());
			if (!exit_state.responsive) {
				error(device.part_block_start_name(), " got stuck");
				device.state = Storage_device::RELEASED;
				reconfigure_runtime = true;
			}
		}

		/* respond to completion of GPT relabeling */
		if (device.relabel_in_progress()) {
			Child_exit_state exit_state(state, device.relabel_start_name());
			if (exit_state.exited) {
				device.rediscover();
				reconfigure_runtime = true;
				_storage.dialog.reset_operation();
				_graph.reset_storage_operation();
			}
		}

		/* respond to completion of GPT expand */
		if (device.gpt_expand_in_progress()) {
			Child_exit_state exit_state(state, device.expand_start_name());
			if (exit_state.exited) {

				/* kick off resize2fs */
				device.for_each_partition([&] (Partition &partition) {
					if (partition.gpt_expand_in_progress) {
						partition.gpt_expand_in_progress = false;
						partition.fs_resize_in_progress  = true;
					}
				});

				reconfigure_runtime = true;
				_storage.dialog.reset_operation();
				_graph.reset_storage_operation();
			}
		}

	}); /* for each device */

	/* handle failed initialization of USB-storage devices */
	_storage._storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device &dev) {
		String<64> name(dev.usb_block_drv_name());
		Child_exit_state exit_state(state, name);
		if (exit_state.exited) {
			dev.discard_usb_block_drv();
			reconfigure_runtime = true;
			regenerate_dialog   = true;
		}
	});

	/* remove prepare subsystem when finished */
	{
		Child_exit_state exit_state(state, "prepare");
		if (exit_state.exited) {
			_prepare_completed = _prepare_version;

			/* trigger update and deploy */
			reconfigure_runtime = true;
		}
	}

	/* schedule pending file operations to new fs_tool instance */
	{
		Child_exit_state exit_state(state, "fs_tool");

		if (exit_state.exited) {

			Child_exit_state::Version const expected_version(_fs_tool_version.value);

			if (exit_state.version == expected_version) {

				_file_operation_queue.schedule_next_operations();
				_fs_tool_version.value++;
				reconfigure_runtime = true;

				/*
				 * The removal of an index file may have completed, re-query index
				 * files to reflect this change at the depot selection menu.
				 */
				if (_popup_dialog.interested_in_file_operations())
					trigger_depot_query();
			}
		}
	}

	/* upgrade RAM and cap quota on demand */
	state.for_each_sub_node("child", [&] (Xml_node child) {

		bool reconfiguration_needed = false;
		_child_states.for_each([&] (Child_state &child_state) {
			if (child_state.apply_child_state_report(child))
				reconfiguration_needed = true; });

		if (reconfiguration_needed) {
			reconfigure_runtime = true;
			regenerate_dialog   = true;
		}
	});

	if (_deploy.update_child_conditions()) {
		reconfigure_runtime = true;
		regenerate_dialog   = true;
	}

	if (regenerate_dialog) {
		generate_dialog();
		_graph_menu_view.generate();
	}

	if (reconfigure_runtime)
		generate_runtime_config();
}


void Sculpt::Main::_generate_runtime_config(Xml_generator &xml) const
{
	xml.attribute("verbose", "yes");

	xml.attribute("prio_levels", _prio_levels.value);

	xml.node("report", [&] () {
		xml.attribute("init_ram",   "yes");
		xml.attribute("init_caps",  "yes");
		xml.attribute("child_ram",  "yes");
		xml.attribute("child_caps", "yes");
		xml.attribute("delay_ms",   4*500);
		xml.attribute("buffer",     "1M");
	});

	xml.node("heartbeat", [&] () { xml.attribute("rate_ms", 2000); });

	xml.node("parent-provides", [&] () {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Rm_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Vm_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Report::Session>(xml);
		gen_parent_service<Platform::Session>(xml);
		gen_parent_service<Block::Session>(xml);
		gen_parent_service<Usb::Session>(xml);
		gen_parent_service<::File_system::Session>(xml);
		gen_parent_service<Gui::Session>(xml);
		gen_parent_service<Rtc::Session>(xml);
		gen_parent_service<Trace::Session>(xml);
		gen_parent_service<Io_mem_session>(xml);
		gen_parent_service<Io_port_session>(xml);
		gen_parent_service<Irq_session>(xml);
		gen_parent_service<Event::Session>(xml);
		gen_parent_service<Capture::Session>(xml);
		gen_parent_service<Gpu::Session>(xml);
		gen_parent_service<Pin_state::Session>(xml);
		gen_parent_service<Pin_control::Session>(xml);
	});

	xml.node("affinity-space", [&] () {
		xml.attribute("width",  _affinity_space.width());
		xml.attribute("height", _affinity_space.height());
	});

	xml.node("start", [&] () {
		gen_runtime_view_start_content(xml, _graph_menu_view._child_state, _font_size_px); });

	_panel_menu_view.gen_start_node(xml);
	_main_menu_view.gen_start_node(xml);
	_settings_menu_view.gen_start_node(xml);
	_network_menu_view.gen_start_node(xml);
	_popup_menu_view.gen_start_node(xml);
	_file_browser_menu_view.gen_start_node(xml);

	_storage.gen_runtime_start_nodes(xml);
	_file_browser_state.gen_start_nodes(xml);

	/*
	 * Load configuration and update depot config on the sculpt partition
	 */
	if (_storage._sculpt_partition.valid() && _prepare_in_progress())
		xml.node("start", [&] () {
			gen_prepare_start_content(xml, _prepare_version); });

	if (_storage.any_file_system_inspected())
		gen_inspect_view(xml, _storage._storage_devices, _storage._ram_fs_state,
		                 _storage._inspect_view_version);

	/*
	 * Spawn chroot instances for accessing '/depot' and '/public'. The
	 * chroot instances implicitly refer to the 'default_fs_rw'.
	 */
	if (_storage._sculpt_partition.valid()) {

		auto chroot = [&] (Start_name const &name, Path const &path, Writeable w) {
			xml.node("start", [&] () {
				gen_chroot_start_content(xml, name, path, w); }); };

		if (_update_running()) {
			chroot("depot_rw",  "/depot",  WRITEABLE);
			chroot("public_rw", "/public", WRITEABLE);
		}

		chroot("depot", "/depot",  READ_ONLY);
	}

	/* execute file operations */
	if (_storage._sculpt_partition.valid())
		if (_file_operation_queue.any_operation_in_progress())
			xml.node("start", [&] () {
				gen_fs_tool_start_content(xml, _fs_tool_version,
				                          _file_operation_queue); });

	_network.gen_runtime_start_nodes(xml);

	if (_update_running())
		xml.node("start", [&] () {
			gen_update_start_content(xml); });

	if (_storage._sculpt_partition.valid() && !_prepare_in_progress()) {
		xml.node("start", [&] () {
			gen_launcher_query_start_content(xml); });

		_deploy.gen_runtime_start_nodes(xml, _prio_levels, _affinity_space);
	}
}


void Sculpt::Main::_generate_event_filter_config(Xml_generator &xml)
{
	auto gen_include = [&] (auto rom) {
		xml.node("include", [&] () {
			xml.attribute("rom", rom); }); };

	xml.node("output", [&] () {
		xml.node("chargen", [&] () {
			xml.node("remap", [&] () {

				auto gen_key = [&] (auto from, auto to) {
					xml.node("key", [&] () {
						xml.attribute("name", from);
						xml.attribute("to",   to); }); };

				gen_key("KEY_CAPSLOCK", "KEY_CAPSLOCK");
				gen_key("KEY_F12",      "KEY_DASHBOARD");
				gen_key("KEY_LEFTMETA", "KEY_SCREEN");
				gen_include("numlock.remap");

				xml.node("merge", [&] () {

					auto gen_input = [&] (auto name) {
						xml.node("input", [&] () {
							xml.attribute("name", name); }); };

					xml.node("accelerate", [&] () {
						xml.attribute("max",                   50);
						xml.attribute("sensitivity_percent", 1000);
						xml.attribute("curve",                127);

						xml.node("button-scroll", [&] () {
							gen_input("ps2");

							xml.node("vertical", [&] () {
								xml.attribute("button", "BTN_MIDDLE");
								xml.attribute("speed_percent", -10); });

							xml.node("horizontal", [&] () {
								xml.attribute("button", "BTN_MIDDLE");
								xml.attribute("speed_percent", -10); });
						});
					});

					xml.node("touch-click", [&] () {
						gen_input("touch"); });

					gen_input("usb");
					gen_input("touch");
					gen_input("sdl");
				});
			});

			auto gen_key = [&] (auto key) {
				gen_named_node(xml, "key", key, [&] () {}); };

			xml.node("mod1", [&] () {
				gen_key("KEY_LEFTSHIFT");
				gen_key("KEY_RIGHTSHIFT"); });

			xml.node("mod2", [&] () {
				gen_key("KEY_LEFTCTRL");
				gen_key("KEY_RIGHTCTRL"); });

			xml.node("mod3", [&] () {
				gen_key("KEY_RIGHTALT");  /* AltGr */ });

			xml.node("mod4", [&] () {
				xml.node("rom", [&] () {
					xml.attribute("name", "capslock"); }); });

			xml.node("repeat", [&] () {
				xml.attribute("delay_ms", 230);
				xml.attribute("rate_ms",   40); });

			using Keyboard_layout = Settings::Keyboard_layout;
			Keyboard_layout::for_each([&] (Keyboard_layout const &layout) {
				if (layout.name == _settings.keyboard_layout)
					gen_include(layout.chargen_file); });

			gen_include("keyboard/special");
		});
	});

	auto gen_policy = [&] (auto label) {
		xml.node("policy", [&] () {
			xml.attribute("label", label);
			xml.attribute("input", label); }); };

	gen_policy("ps2");
	gen_policy("usb");
	gen_policy("touch");
	gen_policy("sdl");
}


void Component::construct(Genode::Env &env)
{
	static Sculpt::Main main(env);
}

