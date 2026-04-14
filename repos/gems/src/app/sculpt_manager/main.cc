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

/* local includes */
#include <model/runtime_state.h>
#include <model/child_exit_state.h>
#include <model/file_operation_queue.h>
#include <model/settings.h>
#include <model/presets.h>
#include <model/screensaver.h>
#include <model/system_state.h>
#include <model/fb_config.h>
#include <model/panorama_config.h>
#include <model/dir_query.h>
#include <view/download_status_widget.h>
#include <view/popup_dialog.h>
#include <view/panel_dialog.h>
#include <view/settings_widget.h>
#include <view/system_dialog.h>
#include <view/file_browser_dialog.h>
#include <view/runtime_diag.h>
#include <drivers.h>
#include <gui.h>
#include <keyboard_focus.h>
#include <network.h>
#include <storage.h>
#include <deploy.h>
#include <graph.h>
#include <vfs.h>

namespace Sculpt { struct Main; }


struct Sculpt::Main : Input_event_handler,
                      Runtime_config_generator,
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
                      Dir_query::Action,
                      Panel_dialog::State,
                      Screensaver::Action,
                      Drivers::Action,
                      Enabled_options::Action
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Sculpt_version const _sculpt_version { _env };

	Vfs _vfs { _env, _heap };

	Build_info const _build_info =
		Build_info::from_node(Attached_rom_dataspace(_env, "build_info").node());

	bool const _mnt_reform = (_build_info.board == "mnt_reform2");
	bool const _mnt_pocket = (_build_info.board == "mnt_pocket");
	bool const _armstone   = (_build_info.board == "imx8mp_armstone");

	Registry<Child_state> _child_states { };

	void _with_child(auto const &name, auto const &fn)
	{
		_child_states.for_each([&] (Child_state &child) {
			if (child.attr.name == name)
				fn(child); });

		_child_states.for_each([&] (Child_state &) { }); /* restore orig. order */
	}

	Input::Seq_number_generator _seq_number_generator { };

	Rom_handler<Main> _panorama {
		_env, "report -> gui/panorama", *this, &Main::_handle_panorama };

	void _handle_panorama(Node const &) { _handle_gui_mode(); }

	Gui::Root _gui_root { _env, _heap, *this, _seq_number_generator };

	System_state _system_state { };

	using Power_features = System_power_widget::Supported;

	Power_features _power_features { .suspend  = false,
	                                 .reset    = (_build_info.board == "pc"),
	                                 .poweroff = false };

	Managed_config<Main> _system_config {
		_env, _heap, "system", "system", *this, &Main::_handle_system_config };

	void _broadcast_system_state()
	{
		_system_config.generate([&] (Generator &g) {
			_system_state.generate(g); });
	}

	void _handle_system_config(Node const &state)
	{
		if (_system_state.apply_config(state).progressed)
			_broadcast_system_state();
	}

	Signal_handler<Main> _gui_mode_handler {
		_env.ep(), *this, &Main::_handle_gui_mode };

	void _handle_gui_mode();

	Rom_handler<Main> _leitzentrale_rom {
		_env, "leitzentrale", *this, &Main::_handle_leitzentrale };

	void _handle_leitzentrale(Node const &leitzentrale)
	{
		bool const orig_leitzentrale_visibile = _leitzentrale_visible;
		_leitzentrale_visible = leitzentrale.attribute_value("enabled", false);

		if (orig_leitzentrale_visibile != _leitzentrale_visible)
			_handle_gui_mode();
	}

	Rom_handler<Main> _config { _env, "config", *this, &Main::_handle_config };

	void _handle_config(Node const &config)
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
		_fb_config_model.suspend_connectors();
		_generate_fb_config();
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

	Managed_config<Main> _font_config {
		_env, _heap, "config", "child/font", *this, &Main::_handle_font_config };

	void _handle_font_config(Node const &config)
	{
		/*
		 * Obtain font size from manually maintained font configuration
		 * so that we can adjust the GUI layout accordingly.
		 */
		config.for_each_sub_node("vfs", [&] (Node const &vfs) {
			vfs.for_each_sub_node("dir", [&] (Node const &dir) {
				if (dir.attribute_value("name", String<16>()) == "font") {
					dir.for_each_sub_node("dir", [&] (Node const &type) {
						if (type.attribute_value("name", String<16>()) == "text") {
							type.for_each_sub_node("ttf", [&] (Node const &ttf) {
								double const px = ttf.attribute_value("size_px", 0.0);
								if (px > 0.0)
									_font_size_px = px; }); } }); } }); });

		_font_size_px = max(_font_size_px, _min_font_size_px);

		_handle_gui_mode();

		/* visibility of font section of settings dialog may have changed */
		_settings_dialog.refresh();

		/* visibility of settings button may have changed */
		_refresh_panel_and_window_layout();
	}

	Vfs::Handler<Main> _event_filter_config {
		_vfs, "/model/child/event_filter", *this, &Main::_handle_event_filter_config };

	void _handle_event_filter_config(Node const &node)
	{
		Settings::Keyboard_layout::Name const orig = _settings.keyboard_layout;

		/* detect updated setting */
		_settings.keyboard_layout = { };
		node.with_optional_sub_node("output", [&] (Node const &node) {
			node.with_optional_sub_node("chargen", [&] (Node const &node) {
				node.for_each_sub_node("include", [&] (Node const &node) {
					Path const rom = node.attribute_value("rom", Path());
					Settings::with_layout_name(rom, [&] (auto const &name) {
						_settings.keyboard_layout = name; }); }); }); });

		/* visibility of the settings dialog may have changed */
		if (orig != _settings.keyboard_layout) {
			_settings_dialog.refresh();
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
		.fb    = _mnt_reform || _mnt_pocket || _armstone,
		.touch = false,
		.wifi  = _mnt_pocket, /* initialized via PCI on Reform */
		.usb   = _mnt_reform || _mnt_pocket || _armstone,
		.mmc   = _mnt_reform || _mnt_pocket || _armstone,
		.modem = false,
		.nic   = _mnt_reform || _mnt_pocket || _armstone,

		.fb_on_dedicated_cpu = _mnt_pocket
	};

	Drivers _drivers { _env, _heap, *this };

	Drivers::Resumed _resumed = _drivers.resumed();

	Board_info::Options _driver_options {
		.display = _mnt_reform || _mnt_pocket || _armstone,
		.usb_net = false,
		.nic     = false,
		.wifi    = _mnt_pocket,
		.suppress {},
		.suspending = false,
	};

	Constructible<Child_state> _usb_hid { };

	bool _usb_storage_acquired = false;

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
		_env, "report -> acpi_support/sleep_states",
		*this, &Main::_handle_acpi_sleep_states };

	void _handle_acpi_sleep_states(Node const &node)
	{
		auto const orig_state = _system_state.state;

		if (_system_state.ready_for_suspended(node))
			_system_state.state = System_state::SUSPENDED;

		if (_system_state.ready_for_restarting_drivers(node)) {
			_system_state.state = System_state::RUNNING;
			_driver_options.suspending = false;
			_drivers.update_options(_driver_options);

			_fb_config_model.resume_connectors();
			_generate_fb_config();
		}

		if (orig_state != _system_state.state)
			_broadcast_system_state();
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
		auto with_storage_devices = [&] (auto const &fn)
		{
			_with_usb_devices([&] (Drivers::Storage_devices::Driver const &usb) {
				_with_ahci_ports([&] (Drivers::Storage_devices::Driver const &ahci) {
					_with_nvme_namespaces([&] (Drivers::Storage_devices::Driver const &nvme) {
						_with_mmc_devices([&] (Drivers::Storage_devices::Driver const &mmc) {
							fn( { .usb  = usb,
							      .ahci = ahci,
							      .nvme = nvme,
							      .mmc  = mmc }); }); }); }); });
		};

		Storage_target const orig_target = _storage._selected_target;

		bool total_progress = false;
		for (bool progress = true; progress; total_progress |= progress) {
			progress = false;
			with_storage_devices([&] (Drivers::Storage_devices const &devices) {
				_config.with_node([&] (Node const &config) {
					progress = _storage.update(config, devices).progressed; }); });

			/* update USB policies for storage devices */
			_usb_config.trigger_update();
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

	Vfs::Handler<Main> _ahci_ports {
		_vfs, "/report/ahci/ports", *this, &Main::_handle_ahci_ports };

	Vfs::Handler<Main> _nvme_namespaces {
		_vfs, "/report/nvme/controller", *this, &Main::_handle_nvme_namespaces };

	Vfs::Handler<Main> _mmc_devices {
		_vfs, "/report/mmc/block_devices", *this, &Main::_handle_mmc_devices };

	Vfs::Handler<Main> _usb_devices {
		_vfs, "/report/usb/devices", *this, &Main::_handle_usb_devices };

	void _handle_ahci_ports     (Node const &) { handle_device_plug_unplug(); }
	void _handle_nvme_namespaces(Node const &) { handle_device_plug_unplug(); }
	void _handle_mmc_devices    (Node const &) { handle_device_plug_unplug(); }

	void _handle_usb_devices(Node const &devices)
	{
		bool const orig_usb_hid_present = _usb_hid.constructed();

		bool usb_hid_present  = false;
		_usb_storage_acquired = false;

		static constexpr unsigned CLASS_HID = 3, CLASS_STORAGE = 8;

		devices.for_each_sub_node("device", [&] (Node const &device) {
			bool const acquired = device.attribute_value("acquired", false);
			device.for_each_sub_node("config", [&] (Node const &config) {
				config.for_each_sub_node("interface", [&] (Node const &interface) {
					unsigned const class_id = interface.attribute_value("class", 0u);
					usb_hid_present       |= (class_id == CLASS_HID);
					_usb_storage_acquired |= (class_id == CLASS_STORAGE) && acquired;
				});
			});
		});

		if (orig_usb_hid_present != usb_hid_present) {
			_usb_hid.conditional(usb_hid_present,
			                     _child_states, Priority::MULTIMEDIA,
			                     Child_name { "usb_hid" }, Binary_name { "usb_hid" },
			                     Ram_quota { 11*1024*1024 }, Cap_quota { 180 });

			generate_runtime_config();
		}

		handle_device_plug_unplug();
	}

	void _with_ahci_ports(auto const &fn) const
	{
		_ahci_ports.with_node([&] (Node const &node) {
			fn({ .present = _runtime_state.present_in_runtime("ahci"), .report = node }); });
	}

	void _with_nvme_namespaces(auto const &fn) const
	{
		_nvme_namespaces.with_node([&] (Node const &node) {
			fn({ .present = _runtime_state.present_in_runtime("nvme"), .report = node }); });
	}

	void _with_mmc_devices(auto const &fn) const
	{
		_mmc_devices.with_node([&] (Node const &node) {
			fn({ .present = _runtime_state.present_in_runtime("mmc"), .report = node }); });
	}

	void _with_usb_devices(auto const &fn) const
	{
		_usb_devices.with_node([&] (Node const &node) {
			fn({ .present = _runtime_state.present_in_runtime("usb"), .report = node }); });
	}

	Storage _storage { _env, _heap, *this };

	void _restart_from_storage_target()
	{
		/* trigger loading of the configuration from the sculpt partition */
		_prepare_version.value++;

		_download_queue.reset();

		_deploy.reset(_vfs);

		_bump_depot_version();
		_query_depot();
	}

	Managed_config<Main> _usb_config {
		_env, _heap, "config", "child/usb", *this, &Main::_handle_usb_config };

	void _handle_usb_config(Node const &config)
	{
		static constexpr unsigned CLASS_HID = 3;

		_usb_config.generate([&] (Generator &g) {
			config.for_each_attribute([&] (Node::Attribute const &a) {
				if (a.name != "managed")
					g.attribute(a.name.string(), a.value.start, a.value.num_bytes); });

			g.node("report", [&] {
				g.attribute("devices", "yes"); });

			g.node("policy", [&] {
				g.attribute("label_prefix", "usb_hid");
				g.attribute("generated", "yes");
				g.node("device", [&] {
					g.attribute("class", CLASS_HID); }); });

			/* copy user-provided rules */
			config.for_each_sub_node("policy", [&] (Node const &policy) {
				if (!policy.attribute_value("generated", false))
					(void)g.append_node(policy, Generator::Max_depth { 5 }); });

			/* wildcard for USB clients with no policy yet */
			g.node("default-policy", [&] { });

			_storage.gen_usb_storage_policies(g);
		});
	}


	/*************
	 ** Network **
	 *************/

	Network _network { _env, _heap, *this, *this };

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
			Runtime_state const &runtime = _main._runtime_state;
			_main._drivers.with_board_info([&] (Board_info const &board_info) {
				s.sub_scope<Frame>([&] (Scope<Frame> &s) {
					if (!runtime.present_in_runtime("nic_router"))
						return;

					_main._network.dialog.view(s, board_info,
						Network_widget::View_attr::from_runtime(runtime));
				});
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
	void nic_target(Network_widget::Target const target) override
	{
		using Target = Network_widget::Target;

		auto disable_if_unused = [&] (auto const &name, Target driver)
		{
			if (_runtime_state.present_in_runtime(name) && driver != target)
				disable_option(name);
		};

		disable_if_unused("wifi",   Target::WIFI);
		disable_if_unused("nic",    Target::NIC);
		disable_if_unused("mobile", Target::MOBILE);

		auto enable_if_targeted = [&] (auto const &name, Target driver)
		{
			if (driver == target && !_runtime_state.present_in_runtime(name))
				enable_option(name);
		};

		enable_if_targeted("wifi",   Target::WIFI);
		enable_if_targeted("nic",    Target::NIC);
		enable_if_targeted("mobile", Target::MOBILE);
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
		_network_dialog.refresh();
		_system_dialog.refresh();
		generate_runtime_config(); /* spawn update if network becomes available */
	}


	/************
	 ** Update **
	 ************/

	Rom_handler<Main> _update_state_rom {
		_env, "report -> update/state", *this, &Main::_handle_update_state };

	Managed_config<Main> _install {
		_env, _heap, "install", "install", *this, &Main::_handle_install };

	void _handle_install(Node const &)
	{
		/* enable update component when manually managing /config/install */
		if (!_install.managed)
			generate_runtime_config();
	}

	void _handle_update_state(Node const &);

	bool _update_needed() const
	{
		return !_install.managed || _download_queue.any_active_download();
	}

	/**
	 * Condition for spawning the update subsystem
	 */
	bool _update_running() const
	{
		return _storage._selected_target.valid()
		    && !_prepare_in_progress()
		    && _network.ready()
		    && _update_needed();
	}

	Download_queue _download_queue { _heap };

	File_operation_queue _file_operation_queue { _heap };

	Fs_tool_version _fs_tool_version { 0 };

	Index_update_queue _index_update_queue {
		_heap, _file_operation_queue, _download_queue };

	void _update_install()
	{
		if (_install.managed)
			_install.generate([&] (Generator &g) {
				g.attribute("arch", _build_info.arch);
				_download_queue.gen_install_entries(g); });
	}


	/*****************
	 ** Depot query **
	 *****************/

	unsigned _depot_query_version { };

	Depot::Archive::User _image_index_user = _build_info.depot_user;

	Depot::Archive::User _index_user = _build_info.depot_user;

	Expanding_reporter _depot_query_reporter { _env, "query", "child/depot_query"};

	bool _system_dialog_watches_depot() const
	{
		return _system_visible && _system_dialog.update_tab_selected();
	}

	void _query_depot()
	{
		_depot_query_version++;
		_depot_query_reporter.generate([&] (Generator &g) {
			g.attribute("arch",    _build_info.arch);
			g.attribute("version", _depot_query_version);

			bool const query_users = _popup_dialog.watches_depot()
			                      || _system_dialog_watches_depot()
			                      || !_scan_rom.valid();
			if (query_users)
				g.node("scan", [&] {
					g.attribute("users", "yes"); });

			if (_popup_dialog.watches_depot() || !_image_index_rom.valid())
				g.node("index", [&] {
					g.attribute("user",    _index_user);
					g.attribute("version", _sculpt_version);
					g.attribute("content", "yes");
				});

			if (_system_dialog_watches_depot() || !_image_index_rom.valid())
				g.node("image_index", [&] {
					g.attribute("os",    "sculpt");
					g.attribute("board", _build_info.board);
					g.attribute("user",  _image_index_user);
				});

			_runtime_state.with_construction([&] (Component const &component) {
				g.node("blueprint", [&] {
					g.attribute("pkg", component.path); }); });
		});
	}

	Rom_handler<Main> _depot_query_blueprint_rom {
		_env, "report -> depot_query/blueprint", *this, &Main::_handle_depot_query_blueprint };

	void _handle_depot_query_blueprint(Node const &blueprint)
	{
		/*
		 * Drop intermediate results that will be superseded by a newer query.
		 * This is important because an outdated blueprint would be disregarded
		 * by 'handle_deploy' anyway while at the same time a new query is
		 * issued. This can result a feedback loop where blueprints are
		 * requested but never applied.
		 */
		if (blueprint.attribute_value("version", 0U) != _depot_query_version)
			return;

		_runtime_state.apply_to_construction([&] (Component &component) {
			component.try_apply_blueprint(blueprint); });

		_popup_dialog.refresh();
	}


	/******************
	 ** Browse index **
	 ******************/

	Rom_handler<Main> _index_rom {
		_env, "report -> depot_query/index", *this, &Main::_handle_index };

	void _handle_index(Node const &)
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
		_query_depot();
	}


	/************
	 ** Deploy **
	 ************/

	Rom_handler<Main> _scan_rom {
		_env, "report -> depot_query/scan", *this, &Main::_handle_scan };

	void _handle_scan(Node const &)
	{
		_system_dialog.sanitize_user_selection();
		_popup_dialog.sanitize_user_selection();
		_popup_dialog.refresh();
	}

	Rom_handler<Main> _image_index_rom {
		_env, "report -> depot_query/image_index", *this, &Main::_handle_image_index };

	void _handle_image_index(Node const &) { _system_dialog.refresh(); }

	Options _options { _heap };
	Presets _presets { _heap };

	Rom_handler<Main> _model_listing_rom {
		_env, "report -> model_query/listing", *this,
		&Main::_handle_model_listing };

	void _handle_model_listing(Node const &listing)
	{
		listing.for_each_sub_node("dir", [&] (Node const &dir) {

			Path const dir_path = dir.attribute_value("path", Path());

			/* iterate over <file> nodes */

			if (dir_path == "/option")  _options.update_from_node(dir);
			if (dir_path == "/presets") _presets.update_from_node(dir);
		});

		_generate_dialog();
	}

	Deploy _deploy { _heap };

	Rom_handler<Main> _deploy_handler {
		_env, "model -> deploy", *this, &Main::_handle_deploy };

	Expanding_reporter _managed_option { _env, "option", "option/managed" };

	void _handle_deploy(Node const &deploy)
	{
		if (!_deploy.apply_deploy(deploy).progressed)
			return;

		_deploy.watch_options(_vfs, *this);

		{
			bool const orig = _log_visible;
			_log_visible = _deploy.enabled_options.info("log_view").exists;
			if (orig != _log_visible)
				_update_window_layout();
		}
	}

	/**
	 * Enabled_options::Action
	 */
	void deploy_option_changed(Options::Name const &name, Node const &node) override
	{
		_deploy.apply_option(name, node);
	}

	Managed_config<Main> _managed_depot_version {
		_env, _heap, "depot", "depot_version", *this, &Main::_handle_depot_version };

	struct Depot_version { unsigned value; } _depot_version { };

	void _handle_depot_version(Node const &node)
	{
		_depot_version.value = node.attribute_value("version", 0u);
	}

	void _bump_depot_version()
	{
		_managed_depot_version.generate([&] (Generator &g) {
			g.attribute("version", _depot_version.value + 1); });
	}


	/************
	 ** Global **
	 ************/

	Settings _settings { };

	double const _min_font_size_px = 6.0;

	double _font_size_px = 14;

	Area  _screen_size { };
	Point _screen_pos  { };

	bool _leitzentrale_visible = false;

	Rom_handler<Main> _nitpicker_hover_handler {
		_env, "nitpicker_hover", *this, &Main::_handle_nitpicker_hover };

	Rom_handler<Main> _clicked_handler {
		_env, "clicked", *this, &Main::_handle_click_report };

	Rom_handler<Main> _touch_handler {
		_env, "touch", *this, &Main::_handle_touch_report };

	Expanding_reporter _gui_fb_config { _env, "config", "gui_fb_config" };

	Constructible<Gui::Point> _pointer_pos { };

	Fb_connectors::Name _hovered_display { };

	static bool _matches_popup_dialog(Node const &node)
	{
		using Label = String<128>;

		Label label  { node.attribute_value("label", Label()) };
		Label suffix { "popup" };

		if (label.length() >= suffix.length()) {
			size_t const offset = label.length() - suffix.length();
			if (!strcmp(label.string() + offset, suffix.string()))
				return true;
		}
		return false;
	}

	/*
	 * The seq-number check ensures the freshness of the _popup_touched and
	 * _popup_hovered information.
	 */
	static bool _seq_number_attr_matches(Node const &node, Input::Seq_number seq)
	{
		return node.has_attribute("seq_number")
		    && (node.attribute_value("seq_number", 0U) == seq.value);
	}

	void _handle_touch_report(Node const &touch)
	{
		if (!_seq_number_attr_matches(touch, _emitted_touch_seq_number))
			_popup_touched = Popup_touched::MAYBE;
		else if (_popup_touched == Popup_touched::MAYBE)
			_popup_touched = _matches_popup_dialog(touch) ? Popup_touched::YES
			                                              : Popup_touched::NO;
		_try_handle_popup_close();
	}

	void _handle_click_report(Node const &click)
	{
		if (!_seq_number_attr_matches(click, _emitted_click_seq_number))
			_popup_clicked = Popup_clicked::MAYBE;
		else if (_popup_clicked == Popup_clicked::MAYBE)
			_popup_clicked = _matches_popup_dialog(click) ? Popup_clicked::YES
			                                              : Popup_clicked::NO;
		_try_handle_popup_close();
	}

	void _handle_nitpicker_hover(Node const &hover)
	{
		if (hover.has_attribute("xpos"))
			_pointer_pos.construct(Gui::Point::from_node(hover));

		/* place leitzentrale at the display under the pointer */
		_handle_gui_mode();
	}

	Panel_dialog::Tab _selected_tab = Panel_dialog::Tab::COMPONENTS;

	bool _log_visible      = false;
	bool _network_visible  = false;
	bool _settings_visible = false;
	bool _system_visible   = false;

	File_browser_state _file_browser_state { };

	Rom_handler<Main> _editor_saved_rom {
		_env, "report -> editor/saved", *this, &Main::_handle_editor_saved };

	Affinity::Space _affinity_space { 1, 1 };

	/**
	 * Panel_dialog::State interface
	 */
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

				bool const network_missing = _main._update_needed()
				                         && !_main._network._nic_state.ready();

				bool const show_diagnostics = _main._cached_init_config.any_stalled()
				                           || network_missing;
				if (show_diagnostics) {

					Hosted<Vbox, Titled_frame> diag { Id { "Diagnostics" } };

					s.widget(diag, [&] {
						if (network_missing)
							s.sub_scope<Left_annotation>("network needed for installation");

						s.as_new_scope([&] (Scope<> &s) {
							view_runtime_diag(s, _alloc, _main._cached_init_config); });
					});
				}

				_main._update_state_rom.with_node([&] (Node const &state) {

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
		_network_dialog.refresh();
		if (_popup.state == Popup::VISIBLE)
			_popup_dialog.refresh();

		if (_system_visible)
			_system_dialog.refresh();
	}

	Rom_handler<Main> _runtime_state_rom {
		_env, "report -> state", *this, &Main::_handle_runtime_state };

	void _handle_runtime_state(Node const &);

	Runtime_state _runtime_state { _heap, _storage._selected_target };

	void _generate_runtime_config(Generator &) const;
	void _generate_managed_option(Generator &) const;

	/**
	 * Runtime_config_generator interface
	 */
	void generate_runtime_config() override
	{
		_managed_option.generate([&] (Generator &g) {
			_generate_managed_option(g); });
	}


	/****************************************
	 ** Cached model of the runtime config **
	 ****************************************/

	/*
	 * Even though the runtime configuration is generated by the sculpt
	 * manager, we still obtain it as a separate ROM session to keep the GUI
	 * part decoupled from the lower-level runtime configuration generator.
	 */
	Rom_handler<Main> _init_config_rom {
		_env, "runtime_config", *this, &Main::_handle_init_config };

	Runtime_config _cached_init_config { _heap };

	void _handle_init_config(Node const &init_config)
	{
		if (_cached_init_config.update_from_node(init_config).stalled())
			return;

		bool download_added = false;
		_cached_init_config.for_each_missing_pkg([&] (Depot::Archive::Path const &path) {
			if (_download_queue.add(path, Verify { true }).progressed)
				download_added = true; });

		if (download_added)
			_update_install();

		bool const reconfigure_runtime =
			_dir_query.update(_heap, _cached_init_config).runtime_reconfig_needed;

		_graph_view.refresh();

		if (_selected_tab == Panel_dialog::Tab::FILES)
			_file_browser_dialog.refresh();

		if (reconfigure_runtime || download_added)
			generate_runtime_config();
	}


	/****************************
	 ** Interactive operations **
	 ****************************/

	Keyboard_focus _keyboard_focus { _env, _network.dialog, _network.wpa_passphrase,
	                                 *this, _system_dialog, _system_visible,
	                                 _popup_dialog, _popup };

	unsigned _key_cnt { 0 };

	struct Keyboard_focus_guard
	{
		Main &_main;

		Keyboard_focus_guard(Main &main, Input::Event const &ev) : _main(main)
		{
			if (ev.press())                         _main._key_cnt++;
			if (ev.release() && _main._key_cnt > 0) _main._key_cnt--;
		}

		~Keyboard_focus_guard() {
			if (_main._key_cnt == 0) _main._keyboard_focus.update(); }
	};

	/* used to prevent closing the popup immediatedly after opened */
	Input::Seq_number _popup_opened_seq_number { };
	Input::Seq_number _popup_closed_seq_number { };

	/* used to correlate clicks with the matching hover report */
	Input::Seq_number _emitted_click_seq_number  { };

	/* used to correlate touch event with touched-session info from nitpicker */
	Input::Seq_number _emitted_touch_seq_number  { };

	enum class Popup_touched { MAYBE, NO, YES } _popup_touched { };
	enum class Popup_clicked { MAYBE, NO, YES } _popup_clicked { };

	/**
	 * Input_event_handler interface
	 */
	void handle_input_event(Input::Event const &ev) override
	{
		ev.handle_absolute_motion([&] (int x, int y) {
			_pointer_pos.construct(x, y); });

		Keyboard_focus_guard focus_guard { *this, ev };

		Dialog::Event::Seq_number const seq_number { _seq_number_generator.value() };

		_dialog_runtime.route_input_event(seq_number, ev);

		/*
		 * Detect clicks outside the popup dialog (for closing it)
		 */
		if (ev.key_press(Input::BTN_LEFT)) {
			_emitted_click_seq_number = { _seq_number_generator.value() };
			_popup_clicked = Popup_clicked::MAYBE;
		}
		if (ev.key_press(Input::BTN_TOUCH)) {
			_emitted_touch_seq_number = { _seq_number_generator.value() };
			_popup_touched = Popup_touched::MAYBE;
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

			int const vscroll_step = int(_screen_size.h) / 3;

			if (ev.key_press(Input::KEY_PAGEUP))   dy =  vscroll_step;
			if (ev.key_press(Input::KEY_PAGEDOWN)) dy = -vscroll_step;

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
		_deploy.reset_ram_fs(_vfs);
	}

	/*
	 * Graph::Action interface
	 */
	void remove_deployed_component(Start_name const &name) override
	{
		_cached_init_config.with_component(name, [&] (Runtime_config::Component const &c) {

			bool const option = (c.option.length() > 1);

			auto const path = option ? Path { "/model/option/", c.option }
			                         : Path { "/model/deploy" };

			_vfs.edit(path, [&] (Hid_edit &edit) {
				edit.remove({ option ? "option" : "deploy", " | : child ", name });
			});

		}, [&] { warning("attempt to remove unknown child '", name, "'"); });
	}

	/*
	 * Graph::Action interface
	 */
	void restart_deployed_component(Start_name const &name) override
	{
		_cached_init_config.with_component(name, [&] (Runtime_config::Component const &c) {

			if (c.option == "managed") {
				_with_child(name, [&] (Child_state &child) {
					child.trigger_restart();
					generate_runtime_config();
				});
				return;
			}

			bool const option = (c.option.length() > 1);

			auto const path = option ? Path { "/model/option/", c.option }
			                         : Path { "/model/deploy" };

			_vfs.edit(path, [&] (Hid_edit &edit) {
				edit.adjust({ option ? "option" : "deploy", " | + child | : version" },
				            0u, [&] (unsigned v) { return v + 1; }); });

		}, [&] { warning("attempt to restart unknown child '", name, "'"); });
	}

	/*
	 * Graph::Action interface
	 */
	void open_popup_dialog(Rect anchor) override
	{
		if (_popup.state == Popup::VISIBLE)
			return;

		_popup_opened_seq_number = { _seq_number_generator.value() };
		_popup_clicked = Popup_clicked::MAYBE;
		_popup_touched = Popup_touched::MAYBE;

		_popup_dialog.refresh();
		_popup.anchor = anchor;
		_popup.state = Popup::VISIBLE;
		_graph_view.refresh();
		_update_window_layout();
	}

	/**
	 * Handle click outside of any dialog to close the popup dialog
	 */
	void _try_handle_popup_close()
	{
		/* skip if already handled */
		if (_popup_closed_seq_number.value == _popup_opened_seq_number.value)
			return;

		bool const popup_opened = (_popup_opened_seq_number.value == _seq_number_generator.value());
		if (popup_opened)
			return;

		if (!Popup::VISIBLE)
			return;

		bool close = false;
		if (_popup_touched == Popup_touched::NO) close = true;
		if (_popup_clicked == Popup_clicked::NO) close = true;

		if (close) {
			_popup_closed_seq_number = _popup_opened_seq_number;
			_close_popup_dialog();
			discard_construction();
		}
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
		_query_depot();
	}

	/**
	 * Software_update_widget::Action interface
	 */
	void trigger_image_download(Path const &path, Verify verify) override
	{
		_download_queue.remove_inactive_downloads();
		_download_queue.add(path, verify);
		_update_install();
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
	 * System_options_widget::Action interface
	 */
	void enable_option(Options::Name const &name) override
	{
		_vfs.edit("/model/deploy", [&] (Hid_edit &edit) {
			edit.append("deploy", [&] (Generator &g) {
				g.node("option", [&] { g.attribute("name", name); }); }); });
	}

	/**
	 * System_options_widget::Action interface
	 */
	void disable_option(Options::Name const &name) override
	{
		_vfs.edit("/model/deploy", [&] (Hid_edit &edit) {
			edit.remove({ "deploy | : option ", name }); });
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

		_vfs.copy({ "/model/presets/", name }, "/model/deploy");
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

		Settings::with_chargen(_settings.keyboard_layout, [&] (Path const &old) {
			Settings::with_chargen(keyboard_layout, [&] (Path const &selected) {
				_vfs.edit("/model/child/event_filter", [&] (Hid_edit &edit) {
					edit.adjust("config | + output | + chargen | + include | : rom", Path(),
						[&] (Path const &v) {
							return (v == old) ? selected : v; }); }); }); });
	}

	/**
	 * Dir_query::Action interface
	 */
	void queried_dir_response() override
	{
		if (_popup.state == Popup::VISIBLE)
			_popup_dialog.refresh();
	}

	/**
	 * Component_add_widget::Action interface
	 */
	void query_directory(Dir_query::Query const &query) override
	{
		if (_dir_query.update_query(_env, *this, _child_states, query).runtime_reconfig_needed)
			generate_runtime_config();
	}

	Dir_query _dir_query { _env, _heap, *this };

	Signal_handler<Main> _fs_query_result_handler {
		_env.ep(), *this, &Main::_handle_fs_query_result };

	void _handle_fs_query_result()
	{
		_file_browser_state.update_query_results();
		_file_browser_dialog.refresh();
	}

	void _handle_editor_saved(Node const &saved)
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
	void browse_file_system(File_browser_state::Fs const &fs) override
	{
		_close_edited_file();

		if (fs == _file_browser_state.browsed) {
			_file_browser_state.browsed = { };
			_file_browser_state.fs_query.destruct();

		} else {
			_file_browser_state.browsed = fs;
			_file_browser_state.path = File_browser_state::Path("/");

			Child_name const child_name = fs.query_name();
			_file_browser_state.fs_query.construct(_child_states, Priority::LEITZENTRALE,
			                                       child_name,
			                                       Binary_name { "fs_query" },
			                                       Ram_quota{8*1024*1024}, Cap_quota{200});

			Service::Name const rom_label("report -> ", child_name, "/listing");

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
				_file_browser_state.text_area.construct(_child_states,
				                                        Priority::LEITZENTRALE,
				                                        Child_name  { "editor" },
				                                        Binary_name { "text_area" },
				                                        Ram_quota{80*1024*1024}, Cap_quota{350});
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

		if (_dir_query.drop_query().runtime_reconfig_needed)
			generate_runtime_config();
	}

	void new_construction(Component::Path const &pkg, Verify verify,
	                      Component::Info const &info) override
	{
		_runtime_state.new_construction(pkg, verify, info, _affinity_space);
		_query_depot();
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
		_update_install();

		generate_runtime_config();
	}

	void discard_construction() override { _runtime_state.reset_construction(); }

	void launch_construction()  override
	{
		_vfs.edit("/model/deploy", [&] (Hid_edit &edit) {
			edit.append("deploy", [&] (Generator &g) {
				_runtime_state.gen_construction(g); }); });

		_runtime_state.reset_construction();

		_close_popup_dialog();
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
	                                            _image_index_rom, _options,
	                                            _deploy.enabled_options, *this };

	Dialog_view<Diag_dialog> _diag_dialog { _dialog_runtime, *this, _heap };

	Dialog_view<Popup_dialog> _popup_dialog { _dialog_runtime, *this,
	                                          _build_info, _sculpt_version,
	                                          _network._nic_state,
	                                          _index_update_queue, _index_rom,
	                                          _download_queue, _cached_init_config,
	                                          _dir_query, _scan_rom, *this };

	Dialog_view<File_browser_dialog> _file_browser_dialog { _dialog_runtime,
	                                                        _cached_init_config,
	                                                        _file_browser_state, *this };

	void _update_window_layout(Node const &, Node const &);

	void _update_window_layout()
	{
		_decorator_margins.with_node([&] (Node const &decorator_margins) {
			_window_list.with_node([&] (Node const &window_list) {
				_update_window_layout(decorator_margins, window_list); }); });
	}

	void _handle_window_layout_or_decorator_margins(Node const &)
	{
		_update_window_layout();
	}

	Rom_handler<Main> _window_list {
		_env, "window_list", *this, &Main::_handle_window_layout_or_decorator_margins };

	Rom_handler<Main> _decorator_margins {
		_env, "decorator_margins", *this, &Main::_handle_window_layout_or_decorator_margins };

	Expanding_reporter _wm_focus       { _env, "focus",          "wm_focus" };
	Expanding_reporter _window_layout  { _env, "window_layout",  "window_layout" };
	Expanding_reporter _resize_request { _env, "resize_request", "resize_request" };

	template <size_t N>
	void _with_window(Node const &window_list, String<N> const &match, auto const &fn)
	{
		window_list.for_each_sub_node("window", [&] (Node const &win) {
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


	/**********************************
	 ** Display driver configuration **
	 **********************************/

	Managed_config<Main> _gui_config {
		_env, _heap, "config", "child/gui", *this, &Main::_handle_gui_config };

	void _handle_gui_config(Node const &node)
	{
		_gui_config.generate([&] (Generator &g) {
			node.for_each_attribute([&] (Node::Attribute const &a) {
				if (a.name != "managed")
					g.attribute(a.name.string(), a.value.start, a.value.num_bytes); });
			node.for_each_sub_node([&] (Node const &sub_node) {
				if (sub_node.has_type("capture")) {
					g.node("capture", [&] {

						/* generate panorama of fb-driver sessions */
						Panorama_config(_fb_config_model).gen_policy_entries(g);

						/* default policy for capture applications like screenshot */
						g.node("default-policy", [&] { });
					});
				} else {
					(void)g.append_node(sub_node, Generator::Max_depth { 5 });
				}
			});
		});
	}

	Panorama_config _panorama_config { };

	Fb_connectors _fb_connectors { };

	Managed_config<Main> _fb_config {
		_env, _heap, "config", "fb", *this, &Main::_handle_fb_config };

	Fb_config _fb_config_model { };

	void _generate_fb_config()
	{
		_fb_config.generate([&] (Generator &g) {
			_fb_config_model.generate_managed_fb(g); });

		/* update nitpicker config if the new fb config affects the panorama */
		Panorama_config const orig = _panorama_config;
		_panorama_config = Panorama_config(_fb_config_model);
		if (orig != Panorama_config(_fb_config_model))
			_gui_config.trigger_update();
	}

	void _handle_fb_config(Node const &node)
	{
		_fb_config_model = { };
		_fb_config_model.import_manual_config(node);
		_fb_config_model.apply_connectors(_fb_connectors);

		_generate_fb_config();
	}

	Vfs::Handler<Main> _intel_fb_connectors_handler {
		_vfs, "/report/intel_fb/connectors", *this, &Main::_handle_fb_connectors };

	Vfs::Handler<Main> _vesa_fb_connectors_handler {
		_vfs, "/report/vesa_fb/connectors", *this, &Main::_handle_fb_connectors };

	void _handle_fb_connectors(Node const &connectors)
	{
		if (_fb_connectors.update(_heap, connectors).progressed) {
			_fb_config_model.apply_connectors(_fb_connectors);
			_generate_fb_config();
			_handle_gui_mode();
			_graph_view.refresh();
		}
	}

	/**
	 * Fb_widget::Action interface
	 */
	void select_fb_mode(Fb_connectors::Name                const &conn,
	                    Fb_connectors::Connector::Mode::Id const &mode) override
	{
		_fb_config_model.select_fb_mode(conn, mode, _fb_connectors);
		_generate_fb_config();
	}

	/**
	 * Fb_widget::Action interface
	 */
	void disable_fb_connector(Fb_connectors::Name const &conn) override
	{
		_fb_config_model.disable_connector(conn);
		_generate_fb_config();
	}

	/**
	 * Fb_widget::Action interface
	 */
	void toggle_fb_merge_discrete(Fb_connectors::Name const &conn) override
	{
		_fb_config_model.toggle_merge_discrete(conn);
		_generate_fb_config();
	}

	/**
	 * Fb_widget::Action interface
	 */
	void swap_fb_connector(Fb_connectors::Name const &conn) override
	{
		_fb_config_model.swap_connector(conn);
		_generate_fb_config();
	}

	/**
	 * Fb_widget::Action interface
	 */
	void fb_brightness(Fb_connectors::Name const &conn, unsigned percent) override
	{
		_fb_config_model.brightness(conn, percent);
		_generate_fb_config();
	}

	/**
	 * Fb_widget::Action interface
	 */
	void fb_rotation(Fb_connectors::Name const &conn, Fb_connectors::Orientation::Rotate r) override
	{
		_fb_config_model.rotation(conn, r);
		_generate_fb_config();
	}

	/**
	 * Fb_widget::Action interface
	 */
	void fb_toggle_flip(Fb_connectors::Name const &conn) override
	{
		_fb_config_model.toggle_flip(conn);
		_generate_fb_config();
	}


	/*******************
	 ** Runtime graph **
	 *******************/

	Popup _popup { };

	Graph _graph { _runtime_state, _cached_init_config, _storage._storage_devices,
	               _storage._selected_target, _storage._ram_fs_state, _fb_connectors,
	               _fb_config_model, _hovered_display, _popup.state };

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
		_init_config_rom.with_node([&] (Node const &config) {
			config.with_optional_sub_node("affinity-space", [&] (Node const &node) {
				_affinity_space = Affinity::Space(node.attribute_value("width",  1U),
				                                  node.attribute_value("height", 1U)); }); });

		_drivers.update_soc(_soc);
		_drivers.update_options(_driver_options);
		_gui_config.trigger_update();

		/*
		 * Generate initial configurations
		 */
		_network.wifi_disconnect();

		_handle_storage_devices();

		generate_runtime_config();
		_generate_dialog();
	}
};


void Sculpt::Main::_update_window_layout(Node const &decorator_margins,
                                         Node const &window_list)
{
	auto gui_mode_ready = [&]
	{
		bool result = false;
		_panorama.with_node([&] (Node const &node) {
			if (node.type() != "empty") result = true; });
		return result;
	};

	/* skip window-layout handling (and decorator activity) while booting */
	if (!gui_mode_ready())
		return;

	struct Decorator_margins
	{
		unsigned top = 0, bottom = 0, left = 0, right = 0;

		Decorator_margins(Node const &node)
		{
			node.with_optional_sub_node("floating", [&] (Node const &floating) {
				top    = floating.attribute_value("top",    0U);
				bottom = floating.attribute_value("bottom", 0U);
				left   = floating.attribute_value("left",   0U);
				right  = floating.attribute_value("right",  0U);
			});
		}
	};

	Decorator_margins const margins { decorator_margins };

	unsigned const log_min_w = 400;

	using Label = String<128>;
	Label const
		logo_label             ("logo"),
		inspect_label          ("inspect"),
		editor_label           ("editor"),
		log_view_label         ("log_view -> log"),
		runtime_view_label     ("runtime_view -> runtime"),
		panel_view_label       ("runtime_view -> panel"),
		diag_view_label        ("runtime_view -> diag"),
		popup_view_label       ("runtime_view -> popup"),
		system_view_label      ("runtime_view -> system"),
		settings_view_label    ("runtime_view -> settings"),
		network_view_label     ("runtime_view -> network"),
		file_browser_view_label("runtime_view -> file_browser");

	auto win_size = [&] (Node const &win) { return Area::from_node(win); };

	unsigned panel_height = 0;
	_with_window(window_list, panel_view_label, [&] (Node const &win) {
		panel_height = win_size(win).h; });

	/* suppress intermediate states during the restart of the panel */
	if (panel_height == 0)
		return;

	/* suppress intermediate boot-time states before the framebuffer driver is up */
	if (!_screen_size.valid())
		return;

	/* area reserved for the panel */
	Rect const panel = Rect(Point(0, 0), Area(_screen_size.w, panel_height));

	/* available space on the right of the menu */
	Rect avail = Rect::compound(Point(0, panel.h()),
	                            Point(_screen_size.w - 1, _screen_size.h - 1));

	Point const log_offset = _log_visible
	                       ? Point(0, 0)
	                       : Point(log_min_w + margins.left + margins.right, 0);

	Point const log_p1(avail.x2() - log_min_w - margins.right + 1 + log_offset.x,
	                   avail.y1() + margins.top);
	Point const log_p2(_screen_size.w - margins.right  - 1 + log_offset.x,
	                   _screen_size.h - margins.bottom - 1);

	/* position of the inspect window */
	Point const inspect_p1(avail.x1() + margins.left, avail.y1() + margins.top);
	Point const inspect_p2(avail.x2() - margins.right - 1,
	                       avail.y2() - margins.bottom - 1);

	auto generate_within_screen_boundary = [&] (auto const &fn)
	{
		_resize_request.generate([&] (Generator &resize_generator) {
			_window_layout.generate([&] (Generator &g) {
				g.node("boundary", [&] {
					g.attribute("width",  _screen_size.w);
					g.attribute("height", _screen_size.h);
					fn(g, resize_generator); }); }); });
	};

	generate_within_screen_boundary([&] (Generator &g, Generator &resize_generator) {

		auto gen_window = [&] (Node const &win, Rect rect) {
			if (rect.valid())
				g.node("window", [&] {
					g.attribute("id",     win.attribute_value("id", 0UL));
					g.attribute("xpos",   rect.x1());
					g.attribute("ypos",   rect.y1());
					g.attribute("width",  rect.w());
					g.attribute("height", rect.h());
					g.attribute("title",  win.attribute_value("label", Label()));
				});
		};

		auto gen_resize = [&] (Node const &win, Area area) {
			if (area.valid())
				resize_generator.node("window", [&] {
					resize_generator.attribute("id",     win.attribute_value("id", 0UL));
					resize_generator.attribute("width",  area.w);
					resize_generator.attribute("height", area.h);
				});
		};

		/* window size limited to space unobstructed by the menu and log */
		auto constrained_win_size = [&] (Node const &win) {

			unsigned const inspect_w = inspect_p2.x - inspect_p1.x,
			               inspect_h = inspect_p2.y - inspect_p1.y;

			Area const size = win_size(win);
			return Area(min(inspect_w, size.w), min(inspect_h, size.h));
		};

		_with_window(window_list, panel_view_label, [&] (Node const &win) {
			gen_window(win, panel); });

		_with_window(window_list, log_view_label, [&] (Node const &win) {
			Rect const rect = Rect::compound(log_p1, log_p2);
			gen_window(win, rect);
			gen_resize(win, rect.area);
		});

		int system_right_xpos = 0;
		if (system_available()) {
			_with_window(window_list, system_view_label, [&] (Node const &win) {
				Area  const size = win_size(win);
				Point const pos  = _system_visible
				                 ? Point(0, avail.y1())
				                 : Point(-size.w, avail.y1());
				gen_window(win, Rect(pos, size));

				if (_system_visible)
					system_right_xpos = size.w;
			});
		}

		_with_window(window_list, settings_view_label, [&] (Node const &win) {
			Area  const size = win_size(win);
			Point const pos  = _settings_visible
			                 ? Point(system_right_xpos, avail.y1())
			                 : Point(-size.w, avail.y1());

			if (_settings.interactive_settings_available())
				gen_window(win, Rect(pos, size));
		});

		_with_window(window_list, network_view_label, [&] (Node const &win) {
			Area  const size = win_size(win);
			Point const pos  = _network_visible
			                 ? Point(log_p1.x - size.w, avail.y1())
			                 : Point(_screen_size.w, avail.y1());
			gen_window(win, Rect(pos, size));
		});

		_with_window(window_list, file_browser_view_label, [&] (Node const &win) {
			if (_selected_tab == Panel_dialog::Tab::FILES) {

				Area  const size = constrained_win_size(win);
				Point const pos  = Rect::compound(inspect_p1, inspect_p2).center(size);

				Point const offset = _file_browser_state.text_area.constructed()
				                   ? Point((2*avail.w())/3 - pos.x, 0)
				                   : Point(0, 0);

				gen_window(win, Rect(pos - offset, size));
			}
		});

		_with_window(window_list, editor_label, [&] (Node const &win) {
			if (_selected_tab == Panel_dialog::Tab::FILES) {
				Area  const size = constrained_win_size(win);
				Point const pos  = Rect::compound(inspect_p1 + Point(400, 0), inspect_p2).center(size);

				Point const offset = _file_browser_state.text_area.constructed()
				                   ? Point(avail.w()/3 - pos.x, 0)
				                   : Point(0, 0);

				gen_window(win, Rect(pos + offset, size));
			}
		});

		_with_window(window_list, diag_view_label, [&] (Node const &win) {
			if (_selected_tab == Panel_dialog::Tab::COMPONENTS) {
				Area  const size = win_size(win);
				Point const pos(0, avail.y2() - size.h);
				gen_window(win, Rect(pos, size));
			}
		});

		auto sanitize_scroll_position = [&] (Area const &win_size, int &scroll_ypos)
		{
			unsigned const inspect_h = unsigned(inspect_p2.y - inspect_p1.y + 1);
			if (win_size.h > inspect_h) {
				int const out_of_view_h = win_size.h - inspect_h;
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
		_with_window(window_list, runtime_view_label, [&] (Node const &win) {
			Area const size = win_size(win);
			Rect const inspect = Rect::compound(inspect_p1, inspect_p2);

			/* center graph if there is enough space, scroll otherwise */
			if (size.h < inspect.h()) {
				runtime_view_pos = inspect.center(size);
			} else {
				sanitize_scroll_position(size, _graph_scroll_ypos);
				runtime_view_pos = { inspect.center(size).x,
				                     int(panel.h()) + _graph_scroll_ypos };
			}
		});

		if (_popup.state == Popup::VISIBLE) {
			_with_window(window_list, popup_view_label, [&] (Node const &win) {
				Area const size = win_size(win);
				Rect const inspect = Rect::compound(inspect_p1, inspect_p2);

				int const x = runtime_view_pos.x + _popup.anchor.x2();

				auto y = [&]
				{
					/* try to vertically align the popup at the '+' button */
					if (size.h < inspect.h()) {
						int const anchor_y = (_popup.anchor.y1() + _popup.anchor.y2())/2;
						int const abs_anchor_y = runtime_view_pos.y + anchor_y;
						return max((int)panel_height, abs_anchor_y - (int)size.h/2);
					} else {
						sanitize_scroll_position(size, _popup_scroll_ypos);
						return int(panel.h()) + _popup_scroll_ypos;
					}
				};
				gen_window(win, Rect(Point(x, y()), size));
			});
		}

		_with_window(window_list, inspect_label, [&] (Node const &win) {
			if (_selected_tab == Panel_dialog::Tab::INSPECT) {
				Rect const rect = Rect::compound(inspect_p1, inspect_p2);
				gen_window(win, rect);
				gen_resize(win, rect.area);
			}
		});

		/*
		 * Position runtime view centered within the inspect area, but allow
		 * the overlapping of the log area. (use the menu view's 'win_size').
		 */
		_with_window(window_list, runtime_view_label, [&] (Node const &win) {
			if (_selected_tab == Panel_dialog::Tab::COMPONENTS)
				gen_window(win, Rect(runtime_view_pos, win_size(win))); });

		_with_window(window_list, logo_label, [&] (Node const &win) {
			Area  const size = win_size(win);
			Point const pos(_screen_size.w - size.w, _screen_size.h - size.h);
			gen_window(win, Rect(pos, size));
		});
	});

	/* define window-manager focus */
	_wm_focus.generate([&] (Generator &g) {

		_window_list.with_node([&] (Node const &list) {
			list.for_each_sub_node("window", [&] (Node const &win) {
				Label const label = win.attribute_value("label", Label());

				if (label == inspect_label && _selected_tab == Panel_dialog::Tab::INSPECT)
					g.node("window", [&] {
						g.attribute("id", win.attribute_value("id", 0UL)); });

				if (label == editor_label && _selected_tab == Panel_dialog::Tab::FILES)
					g.node("window", [&] {
						g.attribute("id", win.attribute_value("id", 0UL)); });
			});
		});
	});
}


void Sculpt::Main::_handle_gui_mode()
{
	/* place leitzentrale at pointed display */
	Rect                const orig_screen_rect { _screen_pos, _screen_size };
	Fb_connectors::Name const orig_hovered_display = _hovered_display;
	{
		Rect rect { };

		_panorama.with_node([&] (Node const &info) {
			rect = Rect::from_node(info); /* entire panorama */

			if (!_pointer_pos.constructed())
				return;

			Gui::Point const at = *_pointer_pos;

			info.for_each_sub_node("capture", [&] (Node const &capture) {
				Rect const display = Rect::from_node(capture);
				if (display.contains(at)) {
					rect = display;
					Session_label label { capture.attribute_value("name", String<64>()) };
					_hovered_display = label.last_element();
				}
			});
		});

		_screen_pos  = rect.at;
		_screen_size = rect.area;
	}

	bool const screen_changed = (orig_screen_rect != Rect { _screen_pos, _screen_size })
	                         || (orig_hovered_display != _hovered_display);

	bool update_runtime_config = false;

	if (screen_changed) {
		_gui_fb_config.generate([&] (Generator &g) {
			g.attribute("xpos",   _screen_pos.x);
			g.attribute("ypos",   _screen_pos.y);
			g.attribute("width",  _screen_size.w);
			g.attribute("height", _screen_size.h);
		});

		_panel_dialog.min_width = _screen_size.w;
		unsigned const menu_width = max((unsigned)(_font_size_px*21.0), 320u);
		_diag_dialog.min_width = menu_width;
		_network_dialog.min_width = menu_width;

		_panel_dialog.refresh();
		_network_dialog.refresh();
		_diag_dialog.refresh();
		_update_window_layout();
		update_runtime_config = true;
	}

	_settings.manual_font_config = !_font_config.managed;

	if (!_settings.manual_font_config) {

		double const orig_font_size_px = _font_size_px;

		_font_size_px = (double)min(_screen_size.w, _screen_size.h) / 60.0;

		if (_settings.font_size == Settings::Font_size::SMALL) _font_size_px *= 0.85;
		if (_settings.font_size == Settings::Font_size::LARGE) _font_size_px *= 1.35;

		/*
		 * Limit lower bound of font size. Otherwise, the glyph rendering
		 * may suffer from division-by-zero problems.
		 */
		_font_size_px = max(_font_size_px, _min_font_size_px);

		if (orig_font_size_px != _font_size_px) {
			_font_config.generate([&] (Generator &g) {
				g.attribute("copy",  true);
				g.attribute("paste", true);
				g.node("vfs", [&] {
					gen_named_node(g, "rom", "Vera.ttf");
					gen_named_node(g, "rom", "VeraMono.ttf");
					gen_named_node(g, "dir", "font", [&] {

						auto gen_ttf_dir = [&] (char const *dir_name,
						                        char const *ttf_path, double size_px) {

							gen_named_node(g, "dir", dir_name, [&] {
								gen_named_node(g, "ttf", "regular", [&] {
									g.attribute("path",    ttf_path);
									g.attribute("size_px", size_px);
									g.attribute("cache",   "256K");
								});
							});
						};

						g.tabular([&] {
							gen_ttf_dir("title",      "/Vera.ttf",     _font_size_px*1.25);
							gen_ttf_dir("text",       "/Vera.ttf",     _font_size_px);
							gen_ttf_dir("annotation", "/Vera.ttf",     _font_size_px*0.8);
							gen_ttf_dir("monospace",  "/VeraMono.ttf", _font_size_px);
						});
					});
				});
				g.node("default-policy", [&] { g.attribute("root", "/font"); });

				auto gen_color = [&] (unsigned index, Color color) {
					g.node("color", [&] {
						g.attribute("index", index);
						g.attribute("value", String<16>(color)); });
				};

				Color const background = Color::rgb(0x1c, 0x22, 0x32);
				Color const foreground = Color::rgb(0xbb, 0xbb, 0xb8);

				g.node("palette", [&] {
					g.tabular([&] {
						gen_color(0, background);
						gen_color(8, background);
						gen_color(7, foreground); }); });
			});
			/* propagate font config of runtime view */
			update_runtime_config = true;
		}
	}

	if (update_runtime_config)
		generate_runtime_config();
}


void Sculpt::Main::_handle_update_state(Node const &update_state)
{
	_download_queue.apply_update_state(update_state);
	_download_queue.remove_completed_downloads();

	_index_update_queue.apply_update_state(update_state);

	bool const installation_complete =
		!update_state.attribute_value("progress", false);

	if (installation_complete) {
		_bump_depot_version();
		_query_depot();
	}

	_generate_dialog();
}


void Sculpt::Main::_handle_runtime_state(Node const &state)
{
	bool reconfigure_runtime = false;
	bool regenerate_dialog   = false;
	bool refresh_storage     = false;

	if (_runtime_state.update_from_state_report(state).progressed)
		regenerate_dialog = true;

	_deploy.manage_resource_requests(_vfs, _runtime_state, _cached_init_config);

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
			Child_exit_state exit_state(state, device.part_block_child_name());
			if (!exit_state.responsive) {
				error(device.part_block_child_name(), " got stuck");
				device.state = Storage_device::RELEASED;
				reconfigure_runtime = true;
				refresh_storage     = true;
			}
		}

		/* respond to completion of GPT relabeling */
		if (device.relabel_in_progress()) {
			Child_exit_state exit_state(state, device.relabel_child_name());
			if (exit_state.exited) {
				device.rediscover();
				reconfigure_runtime = true;
				_reset_storage_dialog_operation();
			}
		}

		/* respond to completion of GPT expand */
		if (device.gpt_expand_in_progress()) {
			Child_exit_state exit_state(state, device.expand_child_name());
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
					_update_install();

				/* update depot-user selection after adding new depot URL */
				if (_system_visible || (_popup.state == Popup::VISIBLE))
					_query_depot();
			}
		}
	}

	if (_system_state.state == System_state::BLANKING &&
	    !_fb_config_model.any_connector_enabled()) {

		_system_state.state = System_state::DRIVERS_STOPPING;
		_broadcast_system_state();

		_driver_options.suspending = true;
		_drivers.update_options(_driver_options);

		reconfigure_runtime = true;
	}

	/* upgrade RAM and cap quota on demand */
	state.for_each_sub_node("child", [&] (Node const &child) {

		bool reconfiguration_needed = false;
		_child_states.for_each([&] (Child_state &child_state) {
			if (child_state.apply_child_state_report(child))
				reconfiguration_needed = true; });

		if (reconfiguration_needed) {
			reconfigure_runtime = true;
			regenerate_dialog   = true;
		}
	});

	/* power-management features depend on optional acpi_support subsystem */
	{
		bool const acpi_support = _cached_init_config.present_in_runtime("acpi_support");
		Power_features const orig_power_features = _power_features;
		_power_features.poweroff = acpi_support;
		_power_features.suspend  = acpi_support;
		if (orig_power_features != _power_features)
			_system_dialog.refresh();
	}

	if (refresh_storage)
		_handle_storage_devices();

	if (regenerate_dialog)
		_generate_dialog();

	if (reconfigure_runtime)
		generate_runtime_config();
}


void Sculpt::Main::_generate_managed_option(Generator &g) const
{
	if (_usb_hid.constructed()) {
		g.node("child", [&] {
			_usb_hid->gen_child_node_content(g);
			g.node("config", [&] {
				g.attribute("capslock_led", "rom");
				g.attribute("numlock_led",  "rom");
			});
			g.tabular_node("connect", [&] {
				g.node("usb", [&] {
					gen_named_node(g, "child", "usb"); });
				connect_report(g);
				connect_report_rom(g, "capslock", "global_keys/capslock");
				connect_report_rom(g, "numlock",  "global_keys/numlock");
				connect_event(g, "usb_hid");
			});
		});
	}

	_storage.gen_child_nodes(g);
	_file_browser_state.gen_child_nodes(g);
	_dir_query.gen_child_nodes(g);

	/*
	 * Load configuration and update depot config on the sculpt partition
	 */
	if (_storage._selected_target.valid() && _prepare_in_progress())
		g.node("child", [&] {
			gen_prepare_child_content(g, _prepare_version); });

	/*
	 * Spawn chroot instances for accessing '/depot' and '/public'. The
	 * chroot instances implicitly refer to the 'default_fs_rw'.
	 */
	if (_storage._selected_target.valid()) {

		auto chroot = [&] (Child_name const &name, Path const &path, Writeable w) {
			g.node("child", [&] {
				gen_chroot_child_content(g, name, path, w); }); };

		if (_update_running()) {
			chroot("depot_rw",  "/depot",  WRITEABLE);
			chroot("public_rw", "/public", WRITEABLE);
		}
	}

	if (_storage.any_file_system_inspected())
		gen_inspect_view(g, _storage._storage_devices, _storage._ram_fs_state,
		                 _storage._inspect_view_version);

	/* execute file operations */
	if (_storage._selected_target.valid())
		if (_file_operation_queue.any_operation_in_progress())
			g.node("child", [&] {
				gen_fs_tool_child_content(g, _fs_tool_version,
				                          _file_operation_queue); });

	if (_update_running())
		g.node("child", [&] {
			gen_update_child_content(g); });
}


void Component::construct(Genode::Env &env)
{
	static Sculpt::Main main(env);
}

