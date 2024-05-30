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
#include <i2c_session/i2c_session.h>
#include <event_session/event_session.h>
#include <capture_session/capture_session.h>
#include <gpu_session/gpu_session.h>
#include <pin_state_session/pin_state_session.h>
#include <pin_control_session/pin_control_session.h>
#include <dialog/distant_runtime.h>

/* included from depot_deploy tool */
#include <children.h>

/* local includes */
#include <model/runtime_state.h>
#include <model/child_exit_state.h>
#include <model/file_operation_queue.h>
#include <model/settings.h>
#include <model/presets.h>
#include <model/screensaver.h>
#include <model/system_state.h>
#include <view/download_status_widget.h>
#include <view/popup_dialog.h>
#include <view/panel_dialog.h>
#include <view/settings_widget.h>
#include <view/system_dialog.h>
#include <view/file_browser_dialog.h>
#include <drivers.h>
#include <gui.h>
#include <keyboard_focus.h>
#include <network.h>
#include <storage.h>
#include <deploy.h>
#include <graph.h>

namespace Sculpt { struct Main; }


struct Sculpt::Main : Input_event_handler,
                      Runtime_config_generator,
                      Deploy::Action,
                      Storage_device::Action,
                      Ram_fs_widget::Action,
                      Network::Action,
                      Network::Info,
                      Graph::Action,
                      Panel_dialog::Action,
                      Network_widget::Action,
                      Settings_widget::Action,
                      System_dialog::Action,
                      File_browser_dialog::Action,
                      Popup_dialog::Action,
                      Component::Construction_info,
                      Depot_query,
                      Panel_dialog::State,
                      Screensaver::Action,
                      Drivers::Info,
                      Drivers::Action
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Sculpt_version const _sculpt_version { _env };

	Build_info const _build_info =
		Build_info::from_xml(Attached_rom_dataspace(_env, "build_info").xml());

	bool const _mnt_reform = (_build_info.board == "mnt_reform2");

	Registry<Child_state> _child_states { };

	void _with_child(auto const &name, auto const &fn)
	{
		_child_states.for_each([&] (Child_state &child) {
			if (child.name() == name)
				fn(child); });

		_child_states.for_each([&] (Child_state &) { }); /* restore orig. order */
	}

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

	System_state _system_state { };

	using Power_features = System_power_widget::Supported;

	Power_features _power_features { .suspend  = false,
	                                 .reset    = (_build_info.board == "pc"),
	                                 .poweroff = false };

	Managed_config<Main> _system_config {
		_env, "system", "system", *this, &Main::_handle_system_config };

	void _broadcast_system_state()
	{
		_system_config.generate([&] (Xml_generator &xml) {
			_system_state.generate(xml); });
	}

	void _handle_system_config(Xml_node const &state)
	{
		if (_system_state.apply_config(state).progress)
			_broadcast_system_state();
	}

	Signal_handler<Main> _gui_mode_handler {
		_env.ep(), *this, &Main::_handle_gui_mode };

	void _handle_gui_mode();

	Rom_handler<Main> _config { _env, "config", *this, &Main::_handle_config };

	void _handle_config(Xml_node const &config)
	{
		Board_info::Options const orig_options = _driver_options;
		_driver_options.suppress.ps2       = !config.attribute_value("ps2",       true);
		_driver_options.suppress.intel_gpu = !config.attribute_value("intel_gpu", true);
		if (orig_options != _driver_options) {
			_drivers.update_options(_driver_options);
			generate_runtime_config();
		}

		_handle_storage_devices();
	}

	Screensaver _screensaver { _env, *this };

	/**
	 * Screensaver::Action interface
	 */
	void screensaver_changed() override
	{
		/* hook for driving the lifetime of the display driver */
	}

	/**
	 * System_power_widget::Action
	 */
	void trigger_suspend() override
	{
		_system_state.state = System_state::BLANKING;
		_broadcast_system_state();
	}

	/**
	 * System_power_widget::Action
	 */
	void trigger_reboot() override
	{
		_system_state.state = System_state::RESET;
		_broadcast_system_state();
	}

	/**
	 * System_power_widget::Action
	 */
	void trigger_power_off() override
	{
		_system_state.state = System_state::POWERED_OFF;
		_broadcast_system_state();
	}

	Managed_config<Main> _fonts_config {
		_env, "config", "fonts", *this, &Main::_handle_fonts_config };

	void _handle_fonts_config(Xml_node const &config)
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
		_settings_dialog.refresh();

		/* visibility of settings button may have changed */
		_refresh_panel_and_window_layout();
	}

	Managed_config<Main> _event_filter_config {
		_env, "config", "event_filter", *this, &Main::_handle_event_filter_config };

	void _generate_event_filter_config(Xml_generator &);

	void _handle_event_filter_config(Xml_node const &)
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

		_settings_dialog.refresh();

		/* visibility of the settings dialog may have changed */
		if (orig_settings_available != _settings.interactive_settings_available()) {
			_refresh_panel_and_window_layout();
			_handle_gui_mode();
		}
	}

	Dialog::Distant_runtime _dialog_runtime { _env };

	template <typename TOP_LEVEL_DIALOG>
	struct Dialog_view : TOP_LEVEL_DIALOG, private Distant_runtime::View
	{
		Dialog_view(Distant_runtime &runtime, auto &&... args)
		: TOP_LEVEL_DIALOG(args...), Distant_runtime::View(runtime, *this) { }

		using Distant_runtime::View::refresh;
		using Distant_runtime::View::min_width;
		using Distant_runtime::View::if_hovered;
	};


	/**********************
	 ** Device discovery **
	 **********************/

	Board_info::Soc _soc {
		.fb    = _mnt_reform,
		.touch = false,
		.wifi  = false, /* initialized via PCI */
		.usb   = _mnt_reform,
		.mmc   = _mnt_reform,
		.modem = false,
		.nic   = _mnt_reform,
	};

	Drivers _drivers { _env, _child_states, *this, *this };

	Drivers::Resumed _resumed = _drivers.resumed();

	Board_info::Options _driver_options {
		.display = _mnt_reform,
		.usb_net = false,
		.nic     = false,
		.wifi    = false,
		.suppress {},
		.suspending = false,
	};

	/**
	 * Drivers::Action
	 */
	void handle_device_plug_unplug() override
	{
		/* drive suspend/resume */
		{
			auto const orig_state = _system_state.state;

			Drivers::Resumed const orig_resumed = _resumed;
			_resumed = _drivers.resumed();

			if (orig_resumed.count != _resumed.count)
				_system_state.state = System_state::ACPI_RESUMING;

			if (_system_state.drivers_stopping() && _drivers.ready_for_suspend())
				_system_state.state = System_state::ACPI_SUSPENDING;

			if (orig_state != _system_state.state)
				_broadcast_system_state();
		}

		_handle_storage_devices();
		network_config_changed();
		generate_runtime_config();
	}

	Rom_handler<Main> _acpi_sleep_states {
		_env, "report -> runtime/acpi_support/sleep_states",
		*this, &Main::_handle_acpi_sleep_states };

	void _handle_acpi_sleep_states(Xml_node const &node)
	{
		auto const orig_state = _system_state.state;

		if (_system_state.ready_for_suspended(node))
			_system_state.state = System_state::SUSPENDED;

		if (_system_state.ready_for_restarting_drivers(node)) {
			_system_state.state = System_state::RUNNING;
			_driver_options.suspending = false;
			_drivers.update_options(_driver_options);
		}

		if (orig_state != _system_state.state)
			_broadcast_system_state();
	}

	/**
	 * Drivers::Info
	 */
	void gen_usb_storage_policies(Xml_generator &xml) const override
	{
		_storage.gen_usb_storage_policies(xml);
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


	/*************
	 ** Storage **
	 *************/

	void _handle_storage_devices()
	{
		Storage_target const orig_target = _storage._selected_target;

		bool total_progress = false;
		for (bool progress = true; progress; total_progress |= progress) {
			progress = false;
			_drivers.with_storage_devices([&] (Drivers::Storage_devices const &devices) {
				_config.with_xml([&] (Xml_node const &config) {
					progress = _storage.update(config,
					                           devices.usb,  devices.ahci,
					                           devices.nvme, devices.mmc).progress; }); });

			/* update USB policies for storage devices */
			_drivers.update_usb();
		}

		if (orig_target != _storage._selected_target)
			_restart_from_storage_target();

		if (total_progress) {
			generate_runtime_config();
			_generate_dialog();
		}
	}

	/**
	 * Storage_device::Action
	 */
	void storage_device_discovered() override { _handle_storage_devices(); }

	Storage _storage { _env, _heap, _child_states, *this };

	void _restart_from_storage_target()
	{
		/* trigger loading of the configuration from the sculpt partition */
		_prepare_version.value++;

		_download_queue.reset();
		_deploy.restart();
	}


	/*************
	 ** Network **
	 *************/

	Network _network { _env, _heap, *this, *this, _child_states, *this };

	/**
	 * Network::Info interface
	 */
	bool ap_list_hovered() const override
	{
		return _network_dialog.if_hovered([&] (Hovered_at const &at) {
			return _network.dialog.ap_list_hovered(at); });
	}

	struct Network_top_level_dialog : Top_level_dialog
	{
		Main &_main;

		Network_top_level_dialog(Main &main)
		: Top_level_dialog("network"), _main(main) { }

		void view(Scope<> &s) const override
		{
			_main._drivers.with_board_info([&] (Board_info const &board_info) {
				s.sub_scope<Frame>([&] (Scope<Frame> &s) {
					_main._network.dialog.view(s, board_info); });
			});
		}

		void click(Clicked_at const &at) override
		{
			_main._network.dialog.click(at, _main);
		}

		void clack(Clacked_at const &) override { }
		void drag (Dragged_at const &) override { }
	};

	Dialog_view<Network_top_level_dialog> _network_dialog { _dialog_runtime, *this };

	/**
	 * Network_widget::Action
	 */
	void nic_target(Nic_target::Type const type) override
	{
		_network.nic_target(type);
		generate_runtime_config();
	}

	/**
	 * Network_widget::Action
	 */
	void wifi_connect(Access_point::Bssid bssid) override
	{
		_network.wifi_connect(bssid);
	}

	/**
	 * Network_widget::Action
	 */
	void wifi_disconnect() override { _network.wifi_disconnect(); }

	/**
	 * Network::Action interface
	 */
	void network_config_changed() override
	{
		Nic_target::Type const type = _network._nic_target.type();
		_driver_options.usb_net = (type == Nic_target::MODEM);
		_driver_options.wifi    = (type == Nic_target::WIFI);
		_driver_options.nic     = (type == Nic_target::WIRED);
		_drivers.update_options(_driver_options);

		_network_dialog.refresh();
		_system_dialog.refresh();
	}


	/************
	 ** Update **
	 ************/

	Rom_handler<Main> _update_state_rom {
		_env, "report -> runtime/update/state", *this, &Main::_handle_update_state };

	void _handle_update_state(Xml_node const &);

	/**
	 * Condition for spawning the update subsystem
	 */
	bool _update_running() const
	{
		return _storage._selected_target.valid()
		    && !_prepare_in_progress()
		    && _network.ready()
		    && _deploy.update_needed();
	}

	Download_queue _download_queue { _heap };

	File_operation_queue _file_operation_queue { _heap };

	Fs_tool_version _fs_tool_version { 0 };

	Index_update_queue _index_update_queue {
		_heap, _file_operation_queue, _download_queue };


	/*****************
	 ** Depot query **
	 *****************/

	Depot_query::Version _query_version { 0 };

	Depot::Archive::User _image_index_user = _build_info.depot_user;

	Depot::Archive::User _index_user = _build_info.depot_user;

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

	bool _system_dialog_watches_depot() const
	{
		return _system_visible && _system_dialog.update_tab_selected();
	}

	void _handle_deferred_depot_query(Duration)
	{
		if (_deploy._arch.valid()) {
			_query_version.value++;
			_depot_query_reporter.generate([&] (Xml_generator &xml) {
				xml.attribute("arch",    _deploy._arch);
				xml.attribute("version", _query_version.value);

				bool const query_users = _popup_dialog.watches_depot()
				                      || _system_dialog_watches_depot()
				                      || !_scan_rom.valid();
				if (query_users)
					xml.node("scan", [&] {
						xml.attribute("users", "yes"); });

				if (_popup_dialog.watches_depot() || !_image_index_rom.valid())
					xml.node("index", [&] {
						xml.attribute("user",    _index_user);
						xml.attribute("version", _sculpt_version);
						xml.attribute("content", "yes");
					});

				if (_system_dialog_watches_depot() || !_image_index_rom.valid())
					xml.node("image_index", [&] {
						xml.attribute("os",    "sculpt");
						xml.attribute("board", _build_info.board);
						xml.attribute("user",  _image_index_user);
					});

				_runtime_state.with_construction([&] (Component const &component) {
					xml.node("blueprint", [&] {
						xml.attribute("pkg", component.path); }); });

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


	/******************
	 ** Browse index **
	 ******************/

	Rom_handler<Main> _index_rom {
		_env, "report -> runtime/depot_query/index", *this, &Main::_handle_index };

	void _handle_index(Xml_node const &)
	{
		if (_popup_dialog.watches_depot())
			_popup_dialog.refresh();
	}

	/**
	 * Software_add_widget::Action interface
	 */
	void query_index(Depot::Archive::User const &user) override
	{
		_index_user = user;
		trigger_depot_query();
	}


	/*********************
	 ** Blueprint query **
	 *********************/

	Rom_handler<Main> _blueprint_rom {
		_env, "report -> runtime/depot_query/blueprint", *this, &Main::_handle_blueprint };

	void _handle_blueprint(Xml_node const &blueprint)
	{
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
			component.try_apply_blueprint(blueprint); });

		_deploy.handle_deploy();
		_popup_dialog.refresh();
	}


	/************
	 ** Deploy **
	 ************/

	Deploy::Prio_levels const _prio_levels { 4 };

	Rom_handler<Main> _scan_rom {
		_env, "report -> runtime/depot_query/scan", *this, &Main::_handle_scan };

	void _handle_scan(Xml_node const &)
	{
		_system_dialog.sanitize_user_selection();
		_popup_dialog.sanitize_user_selection();
		_popup_dialog.refresh();
	}

	Rom_handler<Main> _image_index_rom {
		_env, "report -> runtime/depot_query/image_index", *this, &Main::_handle_image_index };

	void _handle_image_index(Xml_node const &) { _system_dialog.refresh(); }

	Launchers _launchers { _heap };
	Presets   _presets   { _heap };

	Rom_handler<Main> _launcher_listing_rom {
		_env, "report -> /runtime/launcher_query/listing", *this,
		&Main::_handle_launcher_and_preset_listing };

	void _handle_launcher_and_preset_listing(Xml_node const &listing)
	{
		listing.for_each_sub_node("dir", [&] (Xml_node const &dir) {

			Path const dir_path = dir.attribute_value("path", Path());

			if (dir_path == "/launcher")
				_launchers.update_from_xml(dir); /* iterate over <file> nodes */

			if (dir_path == "/presets")
				_presets.update_from_xml(dir);   /* iterate over <file> nodes */
		});

		_popup_dialog.refresh();
		_deploy.handle_deploy();
	}

	Deploy _deploy { _env, _heap, _child_states, _runtime_state, *this, *this, *this,
	                 _launcher_listing_rom, _blueprint_rom, _download_queue };

	/**
	 * Deploy::Action interface
	 */
	void refresh_deploy_dialog() override { _generate_dialog(); }

	Rom_handler<Main> _manual_deploy_rom {
		_env, "config -> deploy", *this, &Main::_handle_manual_deploy };

	void _handle_manual_deploy(Xml_node const &manual_deploy)
	{
		_runtime_state.reset_abandoned_and_launched_children();
		_deploy.use_as_deploy_template(manual_deploy);
		_deploy.update_managed_deploy_config();
	}


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
	bool _system_visible   = false;

	File_browser_state _file_browser_state { };

	Rom_handler<Main> _editor_saved_rom {
		_env, "report -> runtime/editor/saved", *this, &Main::_handle_editor_saved };

	Affinity::Space _affinity_space { 1, 1 };

	/**
	 * Panel_dialog::State interface
	 */
	bool log_visible()         const override { return _log_visible; }
	bool network_visible()     const override { return _network_visible; }
	bool settings_visible()    const override { return _settings_visible; }
	bool system_visible()      const override { return _system_visible; }
	bool inspect_tab_visible() const override { return _storage.any_file_system_inspected(); }

	Panel_dialog::Tab selected_tab() const override { return _selected_tab; }

	bool settings_available() const override { return _settings.interactive_settings_available(); }

	bool system_available() const override
	{
		return _storage._selected_target.valid() && !_prepare_in_progress();
	}

	struct Diag_dialog : Top_level_dialog
	{
		Main const &_main;

		Allocator &_alloc;

		Diag_dialog(Main const &main, Allocator &alloc)
		: Top_level_dialog("diag"), _main(main), _alloc(alloc) { }

		void view(Scope<> &s) const override
		{
			s.sub_scope<Vbox>([&] (Scope<Vbox> &s) {

				if (_main._manually_managed_runtime)
					return;

				bool const network_missing = _main._deploy.update_needed()
				                         && !_main._network._nic_state.ready();

				bool const show_diagnostics = _main._deploy.any_unsatisfied_child()
				                           || network_missing;
				if (show_diagnostics) {

					Hosted<Vbox, Titled_frame> diag { Id { "Diagnostics" } };

					s.widget(diag, [&] {
						if (network_missing)
							s.sub_scope<Left_annotation>("network needed for installation");

						s.as_new_scope([&] (Scope<> &s) { _main._deploy.view_diag(s); });
					});
				}

				_main._update_state_rom.with_xml([&] (Xml_node const &state) {

					bool const download_in_progress =
						_main._update_running() && state.attribute_value("progress", false);

					if (download_in_progress || _main._download_queue.any_failed_download()) {

						Hosted<Vbox, Download_status_widget> download_status { Id { "Download" } };

						s.widget(download_status, state, _main._download_queue);
					}
				});
			});
		}
	};

	void _generate_dialog()
	{
		_diag_dialog.refresh();
		_graph_view.refresh();

		if (_system_visible)
			_system_dialog.refresh();
	}

	Rom_handler<Main> _runtime_state_rom {
		_env, "report -> runtime/state", *this, &Main::_handle_runtime_state };

	void _handle_runtime_state(Xml_node const &);

	Runtime_state _runtime_state { _heap, _storage._selected_target };

	Managed_config<Main> _runtime_config {
		_env, "config", "runtime", *this, &Main::_handle_runtime };

	bool _manually_managed_runtime = false;

	void _handle_runtime(Xml_node const &config)
	{
		_manually_managed_runtime = !config.has_type("empty");
		generate_runtime_config();
		_generate_dialog();
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


	/****************************************
	 ** Cached model of the runtime config **
	 ****************************************/

	/*
	 * Even though the runtime configuration is generated by the sculpt
	 * manager, we still obtain it as a separate ROM session to keep the GUI
	 * part decoupled from the lower-level runtime configuration generator.
	 */
	Rom_handler<Main> _runtime_config_rom {
		_env, "config -> managed/runtime", *this, &Main::_handle_runtime_config };

	Runtime_config _cached_runtime_config { _heap };

	void _handle_runtime_config(Xml_node const &runtime_config)
	{
		_cached_runtime_config.update_from_xml(runtime_config);
		_graph_view.refresh();

		if (_selected_tab == Panel_dialog::Tab::FILES)
			_file_browser_dialog.refresh();
	}


	/****************************
	 ** Interactive operations **
	 ****************************/

	Keyboard_focus _keyboard_focus { _env, _network.dialog, _network.wpa_passphrase,
	                                 *this, _system_dialog, _system_visible,
	                                 _popup_dialog, _popup };

	struct Keyboard_focus_guard
	{
		Main &_main;

		Keyboard_focus_guard(Main &main) : _main(main) { }

		~Keyboard_focus_guard() { _main._keyboard_focus.update(); }
	};

	/* used to prevent closing the popup immediatedly after opened */
	Input::Seq_number _popup_opened_seq_number { };
	Input::Seq_number _clicked_seq_number      { };

	/**
	 * Input_event_handler interface
	 */
	void handle_input_event(Input::Event const &ev) override
	{
		Keyboard_focus_guard focus_guard { *this };

		Dialog::Event::Seq_number const seq_number { _global_input_seq_number.value };

		_dialog_runtime.route_input_event(seq_number, ev);

		/*
		 * Detect clicks outside the popup dialog (for closing it)
		 */
		if (ev.key_press(Input::BTN_LEFT) || ev.touch()) {

			_clicked_seq_number = _global_input_seq_number;

			bool const popup_opened =
				(_popup_opened_seq_number.value == _clicked_seq_number.value);

			bool const popup_hovered =
				_popup_dialog.if_hovered([&] (Hovered_at const &) { return true; });

			if (!popup_hovered && !popup_opened) {
				if (_popup.state == Popup::VISIBLE) {
					_close_popup_dialog();
					discard_construction();
				}
			}
		}

		bool need_generate_dialog = false;

		ev.handle_press([&] (Input::Keycode, Codepoint code) {
			if (_keyboard_focus.target == Keyboard_focus::WPA_PASSPHRASE)
				_network.handle_key_press(code);
			if (_keyboard_focus.target == Keyboard_focus::POPUP)
				_popup_dialog.handle_key(code, *this);
			else if (_system_visible && _system_dialog.keyboard_needed())
				_system_dialog.handle_key(code, *this);

			need_generate_dialog = true;
		});

		auto handle_vertical_scroll = [&] (int &scroll_ypos)
		{
			int dy = 0;

			ev.handle_wheel([&] (int, int y) { dy = y*32; });

			if (ev.key_press(Input::KEY_PAGEUP))   dy =  int(_gui.mode().area.h() / 3);
			if (ev.key_press(Input::KEY_PAGEDOWN)) dy = -int(_gui.mode().area.h() / 3);

			if (dy != 0) {
				scroll_ypos += dy;
				_wheel_handler.local_submit();
			}
		};

		if (_selected_tab == Panel_dialog::Tab::COMPONENTS) {
			if (_popup.state == Popup::VISIBLE)
				handle_vertical_scroll(_popup_scroll_ypos);
			else
				handle_vertical_scroll(_graph_scroll_ypos);
		}

		if (need_generate_dialog) {
			_generate_dialog();
			_popup_dialog.refresh();
		}
	}

	/*
	 * Fs_dialog::Action interface
	 */
	void toggle_inspect_view(Storage_target const &target) override
	{
		_storage.toggle_inspect_view(target);

		/* refresh visibility of inspect tab */
		_panel_dialog.refresh();
		generate_runtime_config();
	}

	void use(Storage_target const &target) override
	{
		Storage_target const orig_target = _storage._selected_target;

		_storage._selected_target = target;
		_system_dialog.reset_update_widget();
		_download_queue.reset();

		if (orig_target != _storage._selected_target)
			_restart_from_storage_target();

		/* hide system panel button and system dialog when "un-using" */
		_panel_dialog.refresh();
		_system_dialog.refresh();
		_update_window_layout();
		generate_runtime_config();
	}

	void _reset_storage_dialog_operation()
	{
		_graph.reset_storage_operation();
	}

	/*
	 * Storage_device_widget::Action interface
	 */
	void format(Storage_target const &target) override
	{
		_storage.format(target);
		generate_runtime_config();
	}

	void cancel_format(Storage_target const &target) override
	{
		_storage.cancel_format(target);
		_reset_storage_dialog_operation();
		generate_runtime_config();
	}

	void expand(Storage_target const &target) override
	{
		_storage.expand(target);
		generate_runtime_config();
	}

	void cancel_expand(Storage_target const &target) override
	{
		_storage.cancel_expand(target);
		_reset_storage_dialog_operation();
		generate_runtime_config();
	}

	void check(Storage_target const &target) override
	{
		_storage.check(target);
		generate_runtime_config();
	}

	void toggle_default_storage_target(Storage_target const &target) override
	{
		_storage.toggle_default_storage_target(target);
		generate_runtime_config();
	}

	void reset_ram_fs() override
	{
		_storage.reset_ram_fs();
		generate_runtime_config();
	}

	/*
	 * Graph::Action interface
	 */
	void remove_deployed_component(Start_name const &name) override
	{
		_runtime_state.abandon(name);

		/* update config/managed/deploy with the component 'name' removed */
		_deploy.update_managed_deploy_config();
	}

	/*
	 * Graph::Action interface
	 */
	void restart_deployed_component(Start_name const &name) override
	{
		auto restart = [&] (auto const &name)
		{
			_with_child(name, [&] (Child_state &child) {
				child.trigger_restart();
				generate_runtime_config();
			});
		};

		if (name == "nic" || name == "wifi")
			restart(name);
		else {

			_runtime_state.restart(name);

			/* update config/managed/deploy with the component 'name' removed */
			_deploy.update_managed_deploy_config();
		}
	}

	/*
	 * Graph::Action interface
	 */
	void open_popup_dialog(Rect anchor) override
	{
		if (_popup.state == Popup::VISIBLE)
			return;

		_popup_opened_seq_number = _clicked_seq_number;

		_popup_dialog.refresh();
		_popup.anchor = anchor;
		_popup.state = Popup::VISIBLE;
		_graph_view.refresh();
		_update_window_layout();
	}

	void _refresh_panel_and_window_layout()
	{
		_panel_dialog.refresh();
		_update_window_layout();
	}

	/**
	 * Depot_users_dialog::Action interface
	 */
	void add_depot_url(Depot_url const &depot_url) override
	{
		using Content = File_operation_queue::Content;

		_file_operation_queue.new_small_file(Path("/rw/depot/", depot_url.user, "/download"),
		                                     Content { depot_url.download });

		if (!_file_operation_queue.any_operation_in_progress())
			_file_operation_queue.schedule_next_operations();

		generate_runtime_config();
	}

	/**
	 * Software_update_widget::Action interface
	 */
	void query_image_index(Depot::Archive::User const &user) override
	{
		_image_index_user = user;
		trigger_depot_query();
	}

	/**
	 * Software_update_widget::Action interface
	 */
	void trigger_image_download(Path const &path, Verify verify) override
	{
		_download_queue.remove_inactive_downloads();
		_download_queue.add(path, verify);
		_deploy.update_installation();
		generate_runtime_config();
	}

	/**
	 * Software_update_widget::Action interface
	 */
	void update_image_index(Depot::Archive::User const &user, Verify verify) override
	{
		_download_queue.remove_inactive_downloads();
		_index_update_queue.remove_inactive_updates();
		_index_update_queue.add(Path(user, "/image/index"), verify);
		generate_runtime_config();
	}

	/**
	 * Software_update_widget::Action interface
	 */
	void install_boot_image(Path const &path) override
	{
		auto install_boot_files = [&] (auto const &subdir)
		{
			_file_operation_queue.copy_all_files(Path("/rw/depot/", path, subdir),
			                                     Path("/rw/boot",         subdir));
		};

		install_boot_files("");

		if (_build_info.board == "pc")
			install_boot_files("/grub"); /* grub.cfg */

		if (!_file_operation_queue.any_operation_in_progress())
			_file_operation_queue.schedule_next_operations();

		generate_runtime_config();
	}

	/**
	 * Software_add_dialog::Action interface
	 */
	void update_sculpt_index(Depot::Archive::User const &user, Verify verify) override
	{
		_download_queue.remove_inactive_downloads();
		_index_update_queue.remove_inactive_updates();
		_index_update_queue.add(Path(user, "/index/", _sculpt_version), verify);
		generate_runtime_config();
	}

	/**
	 * Popup_options_widget::Action interface
	 */
	void enable_optional_component(Path const &launcher) override
	{
		_runtime_state.launch(launcher, launcher);

		/* trigger change of the deployment */
		_deploy.update_managed_deploy_config();
		_download_queue.remove_inactive_downloads();
	}

	/**
	 * Popup_options_widget::Action interface
	 */
	void disable_optional_component(Path const &launcher) override
	{
		_runtime_state.abandon(launcher);

		/* update config/managed/deploy with the component 'name' removed */
		_deploy.update_managed_deploy_config();
		_download_queue.remove_inactive_downloads();
	}

	/*
	 * Panel::Action interface
	 */
	void select_tab(Panel_dialog::Tab tab) override
	{
		_selected_tab = tab;

		if (_selected_tab == Panel_dialog::Tab::FILES)
			_file_browser_dialog.refresh();

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
	 * Panel::Action interface
	 */
	void toggle_system_visibility() override
	{
		_system_visible = !_system_visible;
		_refresh_panel_and_window_layout();
	}

	/**
	 * Software_presets_dialog::Action interface
	 */
	void load_deploy_preset(Presets::Info::Name const &name) override
	{
		_download_queue.remove_inactive_downloads();

		_launcher_listing_rom.with_xml([&] (Xml_node const &listing) {
			listing.for_each_sub_node("dir", [&] (Xml_node const &dir) {
				if (dir.attribute_value("path", Path()) == "/presets") {
					dir.for_each_sub_node("file", [&] (Xml_node const &file) {
						if (file.attribute_value("name", Presets::Info::Name()) == name) {
							file.with_optional_sub_node("config", [&] (Xml_node const &config) {
								_runtime_state.reset_abandoned_and_launched_children();
								_deploy.use_as_deploy_template(config);
								_deploy.update_managed_deploy_config(); }); } }); } }); });
	}

	struct Settings_top_level_dialog : Top_level_dialog
	{
		Main &_main;

		Hosted<Frame, Settings_widget> _hosted { Id { "hosted" }, _main._settings };

		Settings_top_level_dialog(Main &main)
		: Top_level_dialog("settings"), _main(main) { }

		void view(Scope<> &s) const override
		{
			s.sub_scope<Frame>([&] (Scope<Frame> &s) { s.widget(_hosted); });
		}

		void click(Clicked_at const &at) override { _hosted.propagate(at, _main); }
		void clack(Clacked_at const &)   override { }
		void drag (Dragged_at const &)   override { }
	};

	Dialog_view<Settings_top_level_dialog> _settings_dialog { _dialog_runtime, *this };

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
		_file_browser_dialog.refresh();
	}

	void _handle_editor_saved(Xml_node const &saved)
	{
		bool const orig_modified = _file_browser_state.modified;

		_file_browser_state.modified           = saved.attribute_value("modified", false);
		_file_browser_state.last_saved_version = saved.attribute_value("version", 0U);

		if (orig_modified != _file_browser_state.modified)
			_file_browser_dialog.refresh();
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

			Service::Label const rom_label("report -> /runtime/", start_name, "/listing");

			_file_browser_state.query_result.construct(_env, rom_label.string());
			_file_browser_state.query_result->sigh(_fs_query_result_handler);
			_handle_fs_query_result();
		}

		generate_runtime_config();

		_file_browser_dialog.refresh();
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
				                                        Ram_quota{32*1024*1024}, Cap_quota{350});
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
		_popup_dialog.refresh();

		/* remove popup window from window layout */
		_update_window_layout();

		/* reset state of the '+' button */
		_graph_view.refresh();
	}

	void new_construction(Component::Path const &pkg, Verify verify,
	                      Component::Info const &info) override
	{
		_runtime_state.new_construction(pkg, verify, info, _affinity_space);
		trigger_depot_query();
	}

	void _apply_to_construction(Component::Construction_action::Apply_to &fn) override
	{
		_runtime_state.apply_to_construction([&] (Component &c) { fn.apply_to(c); });
	}

	/**
	 * Component::Construction_action interface
	 */
	void trigger_pkg_download() override
	{
		_runtime_state.apply_to_construction([&] (Component &c) {
			_download_queue.add(c.path, c.verify); });

		/* incorporate new download-queue content into update */
		_deploy.update_installation();

		generate_runtime_config();
	}

	void discard_construction() override { _runtime_state.discard_construction(); }

	void launch_construction()  override
	{
		_runtime_state.launch_construction();

		_close_popup_dialog();

		/* trigger change of the deployment */
		_deploy.update_managed_deploy_config();
	}

	/**
	 * Component::Construction_info interface
	 */
	void _with_construction(Component::Construction_info::With const &fn) const override
	{
		_runtime_state.with_construction([&] (Component const &c) { fn.with(c); });
	}

	Dialog_view<Panel_dialog> _panel_dialog { _dialog_runtime, *this, *this };

	Dialog_view<System_dialog> _system_dialog { _dialog_runtime,
	                                            _presets, _build_info, _power_features,
	                                            _network._nic_state,
	                                            _download_queue, _index_update_queue,
	                                            _file_operation_queue, _scan_rom,
	                                            _image_index_rom, *this };

	Dialog_view<Diag_dialog> _diag_dialog { _dialog_runtime, *this, _heap };

	Dialog_view<Popup_dialog> _popup_dialog { _dialog_runtime, *this,
	                                          _build_info, _sculpt_version,
	                                          _launchers, _network._nic_state,
	                                          _index_update_queue, _index_rom,
	                                          _download_queue, _runtime_state,
	                                          _cached_runtime_config, _scan_rom,
	                                          *this };

	Dialog_view<File_browser_dialog> _file_browser_dialog { _dialog_runtime,
	                                                        _cached_runtime_config,
	                                                        _file_browser_state, *this };

	Managed_config<Main> _fb_config {
		_env, "config", "fb", *this, &Main::_handle_fb_config };

	void _handle_fb_config(Xml_node const &node)
	{
		_fb_config.generate([&] (Xml_generator &xml) {
			xml.attribute("system", "yes");
			copy_attributes(xml, node);
			node.for_each_sub_node([&] (Xml_node const &sub_node) {
				copy_node(xml, sub_node, { 5 }); }); });
	}

	void _update_window_layout(Xml_node const &, Xml_node const &);

	void _update_window_layout()
	{
		_decorator_margins.with_xml([&] (Xml_node const &decorator_margins) {
			_window_list.with_xml([&] (Xml_node const &window_list) {
				_update_window_layout(decorator_margins, window_list); }); });
	}

	void _handle_window_layout_or_decorator_margins(Xml_node const &)
	{
		_update_window_layout();
	}

	Rom_handler<Main> _window_list {
		_env, "window_list", *this, &Main::_handle_window_layout_or_decorator_margins };

	Rom_handler<Main> _decorator_margins {
		_env, "decorator_margins", *this, &Main::_handle_window_layout_or_decorator_margins };

	Expanding_reporter _wm_focus      { _env, "focus",         "wm_focus" };
	Expanding_reporter _window_layout { _env, "window_layout", "window_layout" };

	template <size_t N>
	void _with_window(Xml_node window_list, String<N> const &match, auto const &fn)
	{
		window_list.for_each_sub_node("window", [&] (Xml_node win) {
			if (win.attribute_value("label", String<N>()) == match)
				fn(win); });
	}

	int _graph_scroll_ypos = 0;
	int _popup_scroll_ypos = 0;

	/*
	 * Signal handler used for locally triggering a window-layout update
	 *
	 * Wheel events tend to come in batches, the signal handler is used to
	 * defer the call of '_update_window_layout' until all events are processed.
	 */
	Signal_handler<Main> _wheel_handler { _env.ep(), *this, &Main::_update_window_layout };


	/*******************
	 ** Runtime graph **
	 *******************/

	Popup _popup { };

	Graph _graph { _runtime_state, _cached_runtime_config, _storage._storage_devices,
	               _storage._selected_target, _storage._ram_fs_state,
	               _popup.state, _deploy._children };

	struct Graph_dialog : Dialog::Top_level_dialog
	{
		Main &_main;

		Graph_dialog(Main &main) : Top_level_dialog("runtime"), _main(main) { }

		void view(Scope<> &s) const override
		{
			s.sub_scope<Depgraph>(Id { "graph" }, [&] (Scope<Depgraph> &s) {
				_main._graph.view(s); });
		}

		void click(Clicked_at const &at) override { _main._graph.click(at, _main); }
		void clack(Clacked_at const &at) override { _main._graph.clack(at, _main, _main); }
		void drag (Dragged_at const &)   override { }

	} _graph_dialog { *this };

	Dialog::Distant_runtime::View
		_graph_view { _dialog_runtime, _graph_dialog,
		            { .opaque      = false,
		              .background  = { } } };

	Main(Env &env) : _env(env)
	{
		_drivers.update_soc(_soc);
		_gui.input()->sigh(_input_handler);
		_gui.mode_sigh(_gui_mode_handler);
		_handle_gui_mode();
		_fb_config.trigger_update();

		/*
		 * Generate initial configurations
		 */
		_network.wifi_disconnect();
		_update_event_filter_config();

		_handle_storage_devices();

		/*
		 * Read static platform information
		 */
		_drivers.with_platform_info([&] (Xml_node const &platform) {
			platform.with_optional_sub_node("affinity-space", [&] (Xml_node const &node) {
				_affinity_space = Affinity::Space(node.attribute_value("width",  1U),
				                                  node.attribute_value("height", 1U)); }); });

		/*
		 * Generate initial config/managed/deploy configuration
		 */
		generate_runtime_config();
		_generate_dialog();
	}
};


void Sculpt::Main::_update_window_layout(Xml_node const &decorator_margins,
                                         Xml_node const &window_list)
{
	/* skip window-layout handling (and decorator activity) while booting */
	if (!_gui_mode_ready)
		return;

	struct Decorator_margins
	{
		unsigned top = 0, bottom = 0, left = 0, right = 0;

		Decorator_margins(Xml_node const &node)
		{
			node.with_optional_sub_node("floating", [&] (Xml_node const &floating) {
				top    = floating.attribute_value("top",    0U);
				bottom = floating.attribute_value("bottom", 0U);
				left   = floating.attribute_value("left",   0U);
				right  = floating.attribute_value("right",  0U);
			});
		}
	};

	Decorator_margins const margins { decorator_margins };

	unsigned const log_min_w = 400;

	typedef String<128> Label;
	Label const
		inspect_label          ("runtime -> leitzentrale -> inspect"),
		runtime_view_label     ("runtime -> leitzentrale -> runtime_dialog"),
		panel_view_label       ("runtime -> leitzentrale -> panel_dialog"),
		diag_view_label        ("runtime -> leitzentrale -> diag_dialog"),
		popup_view_label       ("runtime -> leitzentrale -> popup_dialog"),
		system_view_label      ("runtime -> leitzentrale -> system_dialog"),
		settings_view_label    ("runtime -> leitzentrale -> settings_dialog"),
		network_view_label     ("runtime -> leitzentrale -> network_dialog"),
		file_browser_view_label("runtime -> leitzentrale -> file_browser_dialog"),
		editor_view_label      ("runtime -> leitzentrale -> editor"),
		logo_label             ("logo");

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

		auto gen_window = [&] (Xml_node const &win, Rect rect) {
			if (rect.valid()) {
				xml.node("window", [&] {
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
		auto constrained_win_size = [&] (Xml_node const &win) {

			unsigned const inspect_w = inspect_p2.x() - inspect_p1.x(),
			               inspect_h = inspect_p2.y() - inspect_p1.y();

			Area const size = win_size(win);
			return Area(min(inspect_w, size.w()), min(inspect_h, size.h()));
		};

		_with_window(window_list, panel_view_label, [&] (Xml_node const &win) {
			gen_window(win, panel); });

		_with_window(window_list, Label("log"), [&] (Xml_node const &win) {
			gen_window(win, Rect(log_p1, log_p2)); });

		int system_right_xpos = 0;
		if (system_available()) {
			_with_window(window_list, system_view_label, [&] (Xml_node const &win) {
				Area  const size = win_size(win);
				Point const pos  = _system_visible
				                 ? Point(0, avail.y1())
				                 : Point(-size.w(), avail.y1());
				gen_window(win, Rect(pos, size));

				if (_system_visible)
					system_right_xpos = size.w();
			});
		}

		_with_window(window_list, settings_view_label, [&] (Xml_node const &win) {
			Area  const size = win_size(win);
			Point const pos  = _settings_visible
			                 ? Point(system_right_xpos, avail.y1())
			                 : Point(-size.w(), avail.y1());

			if (_settings.interactive_settings_available())
				gen_window(win, Rect(pos, size));
		});

		_with_window(window_list, network_view_label, [&] (Xml_node const &win) {
			Area  const size = win_size(win);
			Point const pos  = _network_visible
			                 ? Point(log_p1.x() - size.w(), avail.y1())
			                 : Point(mode.area.w(), avail.y1());
			gen_window(win, Rect(pos, size));
		});

		_with_window(window_list, file_browser_view_label, [&] (Xml_node const &win) {
			if (_selected_tab == Panel_dialog::Tab::FILES) {

				Area  const size = constrained_win_size(win);
				Point const pos  = Rect(inspect_p1, inspect_p2).center(size);

				Point const offset = _file_browser_state.text_area.constructed()
				                   ? Point((2*avail.w())/3 - pos.x(), 0)
				                   : Point(0, 0);

				gen_window(win, Rect(pos - offset, size));
			}
		});

		_with_window(window_list, editor_view_label, [&] (Xml_node const &win) {
			if (_selected_tab == Panel_dialog::Tab::FILES) {
				Area  const size = constrained_win_size(win);
				Point const pos  = Rect(inspect_p1 + Point(400, 0), inspect_p2).center(size);

				Point const offset = _file_browser_state.text_area.constructed()
				                   ? Point(avail.w()/3 - pos.x(), 0)
				                   : Point(0, 0);

				gen_window(win, Rect(pos + offset, size));
			}
		});

		_with_window(window_list, diag_view_label, [&] (Xml_node const &win) {
			if (_selected_tab == Panel_dialog::Tab::COMPONENTS) {
				Area  const size = win_size(win);
				Point const pos(0, avail.y2() - size.h());
				gen_window(win, Rect(pos, size));
			}
		});

		auto sanitize_scroll_position = [&] (Area const &win_size, int &scroll_ypos)
		{
			unsigned const inspect_h = unsigned(inspect_p2.y() - inspect_p1.y() + 1);
			if (win_size.h() > inspect_h) {
				int const out_of_view_h = win_size.h() - inspect_h;
				scroll_ypos = max(scroll_ypos, -out_of_view_h);
				scroll_ypos = min(scroll_ypos, 0);
			} else
				scroll_ypos = 0;
		};

		/*
		 * Calculate centered runtime view within the available main (inspect)
		 * area.
		 */
		Point runtime_view_pos { };
		_with_window(window_list, runtime_view_label, [&] (Xml_node const &win) {
			Area const size = win_size(win);
			Rect const inspect(inspect_p1, inspect_p2);

			/* center graph if there is enough space, scroll otherwise */
			if (size.h() < inspect.h()) {
				runtime_view_pos = inspect.center(size);
			} else {
				sanitize_scroll_position(size, _graph_scroll_ypos);
				runtime_view_pos = { inspect.center(size).x(),
				                     int(panel.h()) + _graph_scroll_ypos };
			}
		});

		if (_popup.state == Popup::VISIBLE) {
			_with_window(window_list, popup_view_label, [&] (Xml_node const &win) {
				Area const size = win_size(win);
				Rect const inspect(inspect_p1, inspect_p2);

				int const x = runtime_view_pos.x() + _popup.anchor.x2();

				auto y = [&]
				{
					/* try to vertically align the popup at the '+' button */
					if (size.h() < inspect.h()) {
						int const anchor_y = (_popup.anchor.y1() + _popup.anchor.y2())/2;
						int const abs_anchor_y = runtime_view_pos.y() + anchor_y;
						return max((int)panel_height, abs_anchor_y - (int)size.h()/2);
					} else {
						sanitize_scroll_position(size, _popup_scroll_ypos);
						return int(panel.h()) + _popup_scroll_ypos;
					}
				};
				gen_window(win, Rect(Point(x, y()), size));
			});
		}

		_with_window(window_list, inspect_label, [&] (Xml_node const &win) {
			if (_selected_tab == Panel_dialog::Tab::INSPECT)
				gen_window(win, Rect(inspect_p1, inspect_p2)); });

		/*
		 * Position runtime view centered within the inspect area, but allow
		 * the overlapping of the log area. (use the menu view's 'win_size').
		 */
		_with_window(window_list, runtime_view_label, [&] (Xml_node const &win) {
			if (_selected_tab == Panel_dialog::Tab::COMPONENTS)
				gen_window(win, Rect(runtime_view_pos, win_size(win))); });

		_with_window(window_list, logo_label, [&] (Xml_node const &win) {
			Area  const size = win_size(win);
			Point const pos(mode.area.w() - size.w(), mode.area.h() - size.h());
			gen_window(win, Rect(pos, size));
		});
	});

	/* define window-manager focus */
	_wm_focus.generate([&] (Xml_generator &xml) {

		_window_list.with_xml([&] (Xml_node const &list) {
			list.for_each_sub_node("window", [&] (Xml_node const &win) {
				Label const label = win.attribute_value("label", Label());

				if (label == inspect_label && _selected_tab == Panel_dialog::Tab::INSPECT)
					xml.node("window", [&] {
						xml.attribute("id", win.attribute_value("id", 0UL)); });

				if (label == editor_view_label && _selected_tab == Panel_dialog::Tab::FILES)
					xml.node("window", [&] {
						xml.attribute("id", win.attribute_value("id", 0UL)); });
			});
		});
	});
}


void Sculpt::Main::_handle_gui_mode()
{
	Framebuffer::Mode const mode = _gui.mode();

	if (mode.area.count() > 1)
		_gui_mode_ready = true;

	_update_window_layout();

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
			xml.node("vfs", [&] {
				gen_named_node(xml, "rom", "Vera.ttf");
				gen_named_node(xml, "rom", "VeraMono.ttf");
				gen_named_node(xml, "dir", "fonts", [&] {

					auto gen_ttf_dir = [&] (char const *dir_name,
					                        char const *ttf_path, double size_px) {

						gen_named_node(xml, "dir", dir_name, [&] {
							gen_named_node(xml, "ttf", "regular", [&] {
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
			xml.node("default-policy", [&] { xml.attribute("root", "/fonts"); });

			auto gen_color = [&] (unsigned index, Color color) {
				xml.node("palette", [&] {
					xml.node("color", [&] {
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
	_panel_dialog.min_width = _screen_size.w();
	unsigned const menu_width = max((unsigned)(_font_size_px*21.0), 320u);
	_diag_dialog.min_width = menu_width;
	_network_dialog.min_width = menu_width;

	/* font size may has changed, propagate fonts config of runtime view */
	generate_runtime_config();
}


void Sculpt::Main::_handle_update_state(Xml_node const &update_state)
{
	_download_queue.apply_update_state(update_state);
	bool const any_completed_download = _download_queue.any_completed_download();
	_download_queue.remove_completed_downloads();

	_index_update_queue.apply_update_state(update_state);

	bool const installation_complete =
		!update_state.attribute_value("progress", false);

	if (installation_complete) {

		_blueprint_rom.with_xml([&] (Xml_node const &blueprint) {
			bool const new_depot_query_needed = blueprint_any_missing(blueprint)
			                                 || blueprint_any_rom_missing(blueprint)
			                                 || any_completed_download;
			if (new_depot_query_needed)
				trigger_depot_query();
		});

		_deploy.reattempt_after_installation();
	}

	_generate_dialog();
}


void Sculpt::Main::_handle_runtime_state(Xml_node const &state)
{
	_runtime_state.update_from_state_report(state);

	bool reconfigure_runtime = false;
	bool regenerate_dialog   = false;
	bool refresh_storage     = false;

	/* check for completed storage operations */
	_storage._storage_devices.for_each([&] (Storage_device &device) {

		device.for_each_partition([&] (Partition &partition) {

			Storage_target const target { device.driver, device.port, partition.number };

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
					_reset_storage_dialog_operation();
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
					_reset_storage_dialog_operation();
				}
			}

			/* respond to completion of file-system resize operation */
			if (partition.fs_resize_in_progress) {
				Child_exit_state exit_state(state, Start_name(target.label(), ".resize2fs"));
				if (exit_state.exited) {
					partition.fs_resize_in_progress = false;
					reconfigure_runtime = true;
					device.rediscover();
					_reset_storage_dialog_operation();
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
				refresh_storage     = true;
			}
		}

		/* respond to completion of GPT relabeling */
		if (device.relabel_in_progress()) {
			Child_exit_state exit_state(state, device.relabel_start_name());
			if (exit_state.exited) {
				device.rediscover();
				reconfigure_runtime = true;
				_reset_storage_dialog_operation();
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
				_reset_storage_dialog_operation();
			}
		}

	}); /* for each device */

	/* handle failed initialization of USB-storage devices */
	_storage._storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device &dev) {
		Child_exit_state exit_state(state, dev.driver);
		if (exit_state.exited) {
			dev.discard_usb_block();
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
			_panel_dialog.refresh(); /* show "System" button */
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
				regenerate_dialog = true;

				/* try to proceed after the first step of an depot-index update */
				unsigned const orig_download_count = _index_update_queue.download_count;
				_index_update_queue.try_schedule_downloads();
				if (_index_update_queue.download_count != orig_download_count)
					_deploy.update_installation();

				/* update depot-user selection after adding new depot URL */
				if (_system_visible || (_popup.state == Popup::VISIBLE))
					trigger_depot_query();
			}
		}
	}

	{
		Child_exit_state exit_state(state, "intel_fb");

		if (exit_state.exited && _system_state.state == System_state::BLANKING) {

			_system_state.state = System_state::DRIVERS_STOPPING;
			_broadcast_system_state();

			_driver_options.suspending = true;
			_drivers.update_options(_driver_options);

			reconfigure_runtime = true;
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

	if (_dialog_runtime.apply_runtime_state(state))
		reconfigure_runtime = true;

	/* power-management features depend on optional acpi_support subsystem */
	{
		bool const acpi_support = _cached_runtime_config.present_in_runtime("acpi_support");
		Power_features const orig_power_features = _power_features;
		_power_features.poweroff = acpi_support;
		_power_features.suspend  = acpi_support && _drivers.suspend_supported();;
		if (orig_power_features != _power_features)
			_system_dialog.refresh();
	}

	if (refresh_storage)
		_handle_storage_devices();

	if (regenerate_dialog) {
		_generate_dialog();
		_graph_view.refresh();
	}

	if (reconfigure_runtime)
		generate_runtime_config();
}


void Sculpt::Main::_generate_runtime_config(Xml_generator &xml) const
{
	xml.attribute("verbose", "yes");

	xml.attribute("prio_levels", _prio_levels.value);

	xml.node("report", [&] {
		xml.attribute("init_ram",   "yes");
		xml.attribute("init_caps",  "yes");
		xml.attribute("child_ram",  "yes");
		xml.attribute("child_caps", "yes");
		xml.attribute("delay_ms",   4*500);
		xml.attribute("buffer",     "1M");
	});

	xml.node("heartbeat", [&] { xml.attribute("rate_ms", 2000); });

	xml.node("parent-provides", [&] {
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
		gen_parent_service<I2c::Session>(xml);
		gen_parent_service<Terminal::Session>(xml);
	});

	xml.node("affinity-space", [&] {
		xml.attribute("width",  _affinity_space.width());
		xml.attribute("height", _affinity_space.height());
	});

	_drivers.gen_start_nodes(xml);
	_dialog_runtime.gen_start_nodes(xml);
	_storage.gen_runtime_start_nodes(xml);
	_file_browser_state.gen_start_nodes(xml);

	/*
	 * Load configuration and update depot config on the sculpt partition
	 */
	if (_storage._selected_target.valid() && _prepare_in_progress())
		xml.node("start", [&] {
			gen_prepare_start_content(xml, _prepare_version); });

	if (_storage.any_file_system_inspected())
		gen_inspect_view(xml, _storage._storage_devices, _storage._ram_fs_state,
		                 _storage._inspect_view_version);

	/*
	 * Spawn chroot instances for accessing '/depot' and '/public'. The
	 * chroot instances implicitly refer to the 'default_fs_rw'.
	 */
	if (_storage._selected_target.valid()) {

		auto chroot = [&] (Start_name const &name, Path const &path, Writeable w) {
			xml.node("start", [&] {
				gen_chroot_start_content(xml, name, path, w); }); };

		if (_update_running()) {
			chroot("depot_rw",  "/depot",  WRITEABLE);
			chroot("public_rw", "/public", WRITEABLE);
		}

		chroot("depot", "/depot",  READ_ONLY);
	}

	/* execute file operations */
	if (_storage._selected_target.valid())
		if (_file_operation_queue.any_operation_in_progress())
			xml.node("start", [&] {
				gen_fs_tool_start_content(xml, _fs_tool_version,
				                          _file_operation_queue); });

	_network.gen_runtime_start_nodes(xml);

	if (_update_running())
		xml.node("start", [&] {
			gen_update_start_content(xml); });

	if (_storage._selected_target.valid() && !_prepare_in_progress()) {
		xml.node("start", [&] {
			gen_launcher_query_start_content(xml); });

		_deploy.gen_runtime_start_nodes(xml, _prio_levels, _affinity_space);
	}
}


void Sculpt::Main::_generate_event_filter_config(Xml_generator &xml)
{
	auto gen_include = [&] (auto rom) {
		xml.node("include", [&] {
			xml.attribute("rom", rom); }); };

	xml.node("output", [&] {
		xml.node("chargen", [&] {
			xml.node("remap", [&] {

				auto gen_key = [&] (auto from, auto to) {
					xml.node("key", [&] {
						xml.attribute("name", from);
						xml.attribute("to",   to); }); };

				gen_key("KEY_CAPSLOCK", "KEY_CAPSLOCK");
				gen_key("KEY_F12",      "KEY_DASHBOARD");
				gen_key("KEY_LEFTMETA", "KEY_SCREEN");
				gen_key("KEY_SYSRQ",    "KEY_PRINT");
				gen_include("numlock.remap");

				xml.node("merge", [&] {

					auto gen_input = [&] (auto name) {
						xml.node("input", [&] {
							xml.attribute("name", name); }); };

					xml.node("accelerate", [&] {
						xml.attribute("max",                   50);
						xml.attribute("sensitivity_percent", 1000);
						xml.attribute("curve",                127);

						xml.node("button-scroll", [&] {
							gen_input("ps2");

							xml.node("vertical", [&] {
								xml.attribute("button", "BTN_MIDDLE");
								xml.attribute("speed_percent", -10); });

							xml.node("horizontal", [&] {
								xml.attribute("button", "BTN_MIDDLE");
								xml.attribute("speed_percent", -10); });
						});
					});

					gen_input("usb");
					gen_input("touchpad");
					gen_input("sdl");
				});
			});

			auto gen_key = [&] (auto key) {
				gen_named_node(xml, "key", key, [&] {}); };

			xml.node("mod1", [&] {
				gen_key("KEY_LEFTSHIFT");
				gen_key("KEY_RIGHTSHIFT"); });

			xml.node("mod2", [&] {
				gen_key("KEY_LEFTCTRL");
				gen_key("KEY_RIGHTCTRL"); });

			xml.node("mod3", [&] {
				gen_key("KEY_RIGHTALT");  /* AltGr */ });

			xml.node("mod4", [&] {
				xml.node("rom", [&] {
					xml.attribute("name", "capslock"); }); });

			xml.node("repeat", [&] {
				xml.attribute("delay_ms", 230);
				xml.attribute("rate_ms",   40); });

			using Keyboard_layout = Settings::Keyboard_layout;
			Keyboard_layout::for_each([&] (Keyboard_layout const &layout) {
				if (layout.name == _settings.keyboard_layout)
					gen_include(layout.chargen_file); });

			gen_include("keyboard/special");
		});
	});

	auto gen_policy = [&] (auto label, auto input) {
		xml.node("policy", [&] {
			xml.attribute("label", label);
			xml.attribute("input", input); }); };

	gen_policy("runtime -> ps2",      "ps2");
	gen_policy("runtime -> usb_hid",  "usb");
	gen_policy("runtime -> touchpad", "touchpad");
	gen_policy("drivers -> sdl",      "sdl");
}


void Component::construct(Genode::Env &env)
{
	static Sculpt::Main main(env);
}

