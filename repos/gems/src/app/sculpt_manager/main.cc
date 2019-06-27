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
#include <nitpicker_session/connection.h>
#include <vm_session/vm_session.h>

/* included from depot_deploy tool */
#include <children.h>

/* local includes */
#include <model/runtime_state.h>
#include <model/child_exit_state.h>
#include <model/file_operation_queue.h>
#include <view/download_status.h>
#include <view/popup_dialog.h>
#include <gui.h>
#include <nitpicker.h>
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
                      Graph::Action,
                      Popup_dialog::Action,
                      Popup_dialog::Construction_info,
                      Depot_query
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Sculpt_version const _sculpt_version { _env };

	Constructible<Nitpicker::Connection> _nitpicker { };

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	void _handle_input()
	{
		_nitpicker->input()->for_each_event([&] (Input::Event const &ev) {
			handle_input_event(ev); });
	}

	Signal_handler<Main> _nitpicker_mode_handler {
		_env.ep(), *this, &Main::_handle_nitpicker_mode };

	void _handle_nitpicker_mode();

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
								float const px = ttf.attribute_value("size_px", 0.0);
								if (px > 0.0)
									_gui.font_size(px); }); } }); } }); });

		_handle_nitpicker_mode();
	}

	Managed_config<Main> _input_filter_config {
		_env, "config", "input_filter", *this, &Main::_handle_input_filter_config };

	void _handle_input_filter_config(Xml_node)
	{
		_input_filter_config.try_generate_manually_managed();
	}

	Attached_rom_dataspace _nitpicker_hover { _env, "nitpicker_hover" };

	Signal_handler<Main> _nitpicker_hover_handler {
		_env.ep(), *this, &Main::_handle_nitpicker_hover };

	void _handle_nitpicker_hover();


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
		_pci_info.wifi_present = false;

		_pci_devices.xml().for_each_sub_node("device", [&] (Xml_node device) {

			/* detect Intel Wireless card */
			if (device.attribute_value("class_code", 0UL) == 0x28000)
				_pci_info.wifi_present = true;
		});
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


	Storage _storage { _env, _heap, *this, *this, *this };

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


	Network _network { _env, _heap, *this, *this, _runtime_state, _pci_info };


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

	/**
	 * Depot_query interface
	 */
	void trigger_depot_query() override
	{
		_query_version.value++;

		if (_deploy._arch.valid()) {
			_depot_query_reporter.generate([&] (Xml_generator &xml) {
				xml.attribute("arch",    _deploy._arch);
				xml.attribute("version", _query_version.value);

				_popup_dialog.gen_depot_query(xml);

				/* update query for blueprints of all unconfigured start nodes */
				_deploy.gen_depot_query(xml);
			});
		}
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

		_runtime_state.apply_to_construction([&] (Component &component) {
			_popup_dialog.apply_blueprint(component, blueprint); });

		_deploy.handle_deploy();
	}


	/************
	 ** Deploy **
	 ************/

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

		_popup_dialog.generate();
		_deploy._handle_managed_deploy();
	}


	Deploy _deploy { _env, _heap, _runtime_state, *this, *this, *this,
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

	Gui _gui { _env };

	Expanding_reporter _menu_dialog_reporter { _env, "dialog", "menu_dialog" };

	Attached_rom_dataspace _hover_rom { _env, "menu_view_hover" };

	Signal_handler<Main> _hover_handler {
		_env.ep(), *this, &Main::_handle_hover };

	struct Hovered { enum Dialog { NONE, LOGO, STORAGE, NETWORK, RUNTIME } value; };

	Hovered::Dialog _hovered_dialog { Hovered::NONE };

	template <typename FN>
	void _apply_to_hovered_dialog(Hovered::Dialog dialog, FN const &fn)
	{
		if (dialog == Hovered::STORAGE) fn(_storage.dialog);
		if (dialog == Hovered::NETWORK) fn(_network.dialog);
	}

	void _handle_hover();

	/**
	 * Dialog::Generator interface
	 */
	void generate_dialog() override
	{
		_menu_dialog_reporter.generate([&] (Xml_generator &xml) {

			xml.node("vbox", [&] () {
				gen_named_node(xml, "frame", "logo", [&] () {
					xml.node("float", [&] () {
						xml.node("frame", [&] () {
							xml.attribute("style", "logo"); }); }); });

				if (_manually_managed_runtime)
					return;

				bool const storage_dialog_expanded = _last_clicked == Hovered::STORAGE
				                                  || !_storage.any_file_system_inspected();

				_storage.dialog.generate(xml, storage_dialog_expanded);
				_network.dialog.generate(xml);

				gen_named_node(xml, "frame", "runtime", [&] () {
					xml.node("vbox", [&] () {
						gen_named_node(xml, "label", "title", [&] () {
							xml.attribute("text", "Runtime");
							xml.attribute("font", "title/regular");
						});

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
				});
			});
		});
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
		_graph.gen_dialog();
	}


	/****************************
	 ** Interactive operations **
	 ****************************/

	Keyboard_focus _keyboard_focus { _env, _network.dialog, _network.wpa_passphrase };

	Hovered::Dialog _last_clicked { Hovered::NONE };

	/**
	 * Input_event_handler interface
	 */
	void handle_input_event(Input::Event const &ev) override
	{
		bool need_generate_dialog = false;

		if (ev.key_press(Input::BTN_LEFT)) {

			if (_hovered_dialog != _last_clicked && _hovered_dialog != Hovered::NONE) {
				_last_clicked = _hovered_dialog;
				_handle_window_layout();
				need_generate_dialog = true;
			}

			if (_hovered_dialog == Hovered::STORAGE) _storage.dialog.click(_storage);
			if (_hovered_dialog == Hovered::NETWORK) _network.dialog.click(_network);
			if (_hovered_dialog == Hovered::RUNTIME) _network.dialog.click(_network);

			/* remove popup dialog when clicking somewhere outside */
			if (!_popup_dialog.hovered() && _popup.state == Popup::VISIBLE
			 && !_graph.add_button_hovered()) {

				_popup.state = Popup::OFF;
				_popup_dialog.reset();
				discard_construction();

				/* de-select '+' button */
				_graph._gen_graph_dialog();

				/* remove popup window from window layout */
				_handle_window_layout();
			}

			if (_graph.hovered())        _graph.click(*this);
			if (_popup_dialog.hovered()) _popup_dialog.click(*this);
		}

		if (ev.key_release(Input::BTN_LEFT)) {
			if (_hovered_dialog == Hovered::STORAGE) _storage.dialog.clack(_storage);

			if (_graph.hovered())        _graph.clack(*this);
			if (_popup_dialog.hovered()) _popup_dialog.clack(*this);
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
	void toggle_launcher_selector(Rect anchor) override
	{
		_popup_dialog.generate();
		_popup.anchor = anchor;
		_popup.toggle();
		_graph._gen_graph_dialog();
		_handle_window_layout();
	}

	void _close_popup_dialog()
	{
		/* close popup menu */
		_popup.state = Popup::OFF;
		_popup_dialog.reset();
		_handle_window_layout();

		/* reset state of the '+' button */
		_graph._gen_graph_dialog();
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
		return _runtime_state.new_construction(pkg, info);
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

	Popup_dialog _popup_dialog { _env, _heap, _launchers,
	                             _network._nic_state, _network._nic_target,
	                             _runtime_state, _cached_runtime_config,
	                             _download_queue, *this, *this };

	Managed_config<Main> _fb_drv_config {
		_env, "config", "fb_drv", *this, &Main::_handle_fb_drv_config };

	void _handle_fb_drv_config(Xml_node)
	{
		_fb_drv_config.try_generate_manually_managed();
	}

	Attached_rom_dataspace _nitpicker_displays { _env, "displays" };

	Signal_handler<Main> _nitpicker_displays_handler {
		_env.ep(), *this, &Main::_handle_nitpicker_displays };

	void _handle_nitpicker_displays()
	{
		_nitpicker_displays.update();

		if (!_nitpicker_displays.xml().has_sub_node("display"))
			return;

		if (_nitpicker.constructed())
			return;

		/*
		 * Since nitpicker has successfully issued the first 'displays' report,
		 * there is a good chance that the framebuffer driver is running. This
		 * is a good time to activate the GUI.
		 */
		_nitpicker.construct(_env, "input");
		_nitpicker->input()->sigh(_input_handler);
		_nitpicker->mode_sigh(_nitpicker_mode_handler);

		/*
		 * Adjust GUI parameters to initial nitpicker mode
		 */
		_handle_nitpicker_mode();

		/*
		 * Avoid 'Constructible<Nitpicker::Root>' because it requires the
		 * definition of 'Nitpicker::Session_component'.
		 */
		static Nitpicker::Root gui_nitpicker(_env, _heap, *this);

		_gui.generate_config();
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

	Graph _graph { _env, _runtime_state, _cached_runtime_config,
	               _storage._sculpt_partition, _popup.state, _deploy._children };

	Child_state _runtime_view_state {
		"runtime_view", Ram_quota{8*1024*1024}, Cap_quota{200} };


	Main(Env &env) : _env(env)
	{
		_manual_deploy_rom.sigh(_manual_deploy_handler);
		_runtime_state_rom.sigh(_runtime_state_handler);
		_runtime_config_rom.sigh(_runtime_config_handler);
		_nitpicker_displays.sigh(_nitpicker_displays_handler);

		/*
		 * Subscribe to reports
		 */
		_update_state_rom    .sigh(_update_state_handler);
		_nitpicker_hover     .sigh(_nitpicker_hover_handler);
		_hover_rom           .sigh(_hover_handler);
		_pci_devices         .sigh(_pci_devices_handler);
		_window_list         .sigh(_window_list_handler);
		_decorator_margins   .sigh(_decorator_margins_handler);
		_launcher_listing_rom.sigh(_launcher_listing_handler);
		_blueprint_rom       .sigh(_blueprint_handler);

		/*
		 * Generate initial configurations
		 */
		_network.wifi_disconnect();

		/*
		 * Import initial report content
		 */
		_storage.handle_storage_devices_update();
		_deploy.handle_deploy();
		_handle_pci_devices();
		_handle_runtime_config();

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
	struct Decorator_margins
	{
		unsigned top = 0, bottom = 0, left = 0, right = 0;

		Decorator_margins(Xml_node node)
		{
			if (!node.has_sub_node("floating"))
				return;

			Xml_node const floating = node.sub_node("floating");

			top    = floating.attribute_value("top",    0UL);
			bottom = floating.attribute_value("bottom", 0UL);
			left   = floating.attribute_value("left",   0UL);
			right  = floating.attribute_value("right",  0UL);
		}
	};

	/* read decorator margins from the decorator's report */
	_decorator_margins.update();
	Decorator_margins const margins(_decorator_margins.xml());

	unsigned const log_min_w = 400, log_min_h = 200;

	if (!_nitpicker.constructed())
		return;

	Framebuffer::Mode const mode = _nitpicker->mode();

	/* area preserved for the menu */
	Rect const menu(Point(0, 0), Area(_gui.menu_width, mode.height()));

	/* available space on the right of the menu */
	Rect avail(Point(_gui.menu_width, 0),
	           Point(mode.width() - 1, mode.height() - 1));

	/*
	 * When the screen width is at least twice the log width, place the
	 * log at the right side of the screen. Otherwise, with resolutions
	 * as low as 1024x768, place it to the bottom to allow the inspect
	 * window to use the available screen width to the maximum extend.
	 */
	bool const log_at_right =
		(avail.w() > 2*(log_min_w + margins.left + margins.right));

	/* the upper-left point depends on whether the log is at the right or bottom */
	Point const log_p1 =
		log_at_right ? Point(avail.x2() - log_min_w - margins.right + 1,
		                     margins.top)
		             : Point(_gui.menu_width + margins.left,
		                     avail.y2() - log_min_h - margins.bottom + 1);

	/* the lower-right point (p2) of the log is always the same */
	Point const log_p2(mode.width()  - margins.right  - 1,
	                   mode.height() - margins.bottom - 1);

	/* position of the inspect window */
	Point const inspect_p1(avail.x1() + margins.right, margins.top);

	Point const inspect_p2 =
		log_at_right ? Point(log_p1.x() - margins.right - margins.left - 1, log_p2.y())
		             : Point(log_p2.x(), log_p1.y() - margins.bottom - margins.top - 1);

	typedef String<128> Label;
	Label const inspect_label     ("runtime -> leitzentrale -> inspect");
	Label const runtime_view_label("runtime -> leitzentrale -> runtime_view");

	_window_list.update();
	_window_layout.generate([&] (Xml_generator &xml) {

		Xml_node const window_list = _window_list.xml();

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

		auto win_size = [&] (Xml_node win) {
			return Area(win.attribute_value("width",  0UL),
			            win.attribute_value("height", 0UL)); };

		/* window size limited to space unobstructed by the menu and log */
		auto constrained_win_size = [&] (Xml_node win) {

			unsigned const inspect_w = inspect_p2.x() - inspect_p1.x(),
			               inspect_h = inspect_p2.y() - inspect_p1.y();

			Area const size = win_size(win);
			return Area(min(inspect_w, size.w()), min(inspect_h, size.h()));
		};

		_with_window(window_list, Label("gui -> menu -> "), [&] (Xml_node win) {
			gen_window(win, menu); });

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
			_with_window(window_list, Label("gui -> popup -> "), [&] (Xml_node win) {
				Area const size = win_size(win);

				int const anchor_y_center = (_popup.anchor.y1() + _popup.anchor.y2())/2;

				int const x = runtime_view_pos.x() + _popup.anchor.x2();
				int const y = max(0, runtime_view_pos.y() + anchor_y_center - (int)size.h()/2);

				gen_window(win, Rect(Point(x, y), size));
			});
		}

		if (_last_clicked == Hovered::STORAGE)
			_with_window(window_list, inspect_label, [&] (Xml_node win) {
				gen_window(win, Rect(inspect_p1, inspect_p2)); });

		/*
		 * Position runtime view centered within the inspect area, but allow
		 * the overlapping of the log area. (use the menu view's 'win_size').
		 */
		_with_window(window_list, runtime_view_label, [&] (Xml_node win) {
			gen_window(win, Rect(runtime_view_pos, win_size(win))); });

		_with_window(window_list, Label("log"), [&] (Xml_node win) {
			gen_window(win, Rect(log_p1, log_p2)); });
	});

	/* define window-manager focus */
	_wm_focus.generate([&] (Xml_generator &xml) {
		_window_list.xml().for_each_sub_node("window", [&] (Xml_node win) {
			Label const label = win.attribute_value("label", Label());
			if (label == inspect_label)
				xml.node("window", [&] () {
					xml.attribute("id", win.attribute_value("id", 0UL)); });
		});
	});
}


void Sculpt::Main::_handle_nitpicker_mode()
{
	if (!_nitpicker.constructed())
		return;

	Framebuffer::Mode const mode = _nitpicker->mode();

	_handle_window_layout();

	if (!_fonts_config.try_generate_manually_managed()) {

		float const text_size = (float)mode.height() / 60.0;

		_gui.font_size(text_size);

		_fonts_config.generate([&] (Xml_generator &xml) {
			xml.attribute("copy",  true);
			xml.attribute("paste", true);
			xml.node("vfs", [&] () {
				gen_named_node(xml, "rom", "Vera.ttf");
				gen_named_node(xml, "rom", "VeraMono.ttf");
				gen_named_node(xml, "dir", "fonts", [&] () {

					auto gen_ttf_dir = [&] (char const *dir_name,
					                        char const *ttf_path, float size_px) {

						gen_named_node(xml, "dir", dir_name, [&] () {
							gen_named_node(xml, "ttf", "regular", [&] () {
								xml.attribute("path",    ttf_path);
								xml.attribute("size_px", size_px);
								xml.attribute("cache",   "256K");
							});
						});
					};

					gen_ttf_dir("title",      "/Vera.ttf",     text_size*1.25);
					gen_ttf_dir("text",       "/Vera.ttf",     text_size);
					gen_ttf_dir("annotation", "/Vera.ttf",     text_size*0.8);
					gen_ttf_dir("monospace",  "/VeraMono.ttf", text_size);
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

	_gui.version.value++;
	_gui.generate_config();
}


void Sculpt::Main::_handle_hover()
{
	_hover_rom.update();
	Xml_node const hover = _hover_rom.xml();

	Hovered::Dialog const orig_hovered_dialog = _hovered_dialog;

	typedef String<32> Top_level_frame;
	Top_level_frame const top_level_frame =
		query_attribute<Top_level_frame>(hover, "dialog", "vbox", "frame", "name");

	_hovered_dialog = Hovered::NONE;
	if (top_level_frame == "network") _hovered_dialog = Hovered::NETWORK;
	if (top_level_frame == "storage") _hovered_dialog = Hovered::STORAGE;
	if (top_level_frame == "runtime") _hovered_dialog = Hovered::RUNTIME;
	if (top_level_frame == "logo")    _hovered_dialog = Hovered::LOGO;

	if (orig_hovered_dialog != _hovered_dialog)
		_apply_to_hovered_dialog(orig_hovered_dialog, [&] (Dialog &dialog) {
			dialog.hover(Xml_node("<hover/>")); });

	_apply_to_hovered_dialog(_hovered_dialog, [&] (Dialog &dialog) {
		dialog.hover(hover.sub_node("dialog").sub_node("vbox")
		                                     .sub_node("frame")); });
}


void Sculpt::Main::_handle_nitpicker_hover()
{
	if (!_storage._discovery_state.discovery_in_progress())
		return;

	/* check if initial user activity has already been evaluated */
	if (_storage._discovery_state.user_state != Discovery_state::USER_UNKNOWN)
		return;

	_nitpicker_hover.update();
	Xml_node const hover = _nitpicker_hover.xml();
	if (!hover.has_type("hover"))
		return;

	_storage._discovery_state.user_state = hover.attribute_value("active", false)
	                                     ? Discovery_state::USER_INTERVENED
	                                     : Discovery_state::USER_IDLE;

	/* trigger re-evaluation of default storage target */
	_storage.handle_storage_devices_update();
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

	Xml_node const blueprint = _blueprint_rom.xml();
	bool const new_depot_query_needed = popup_watches_downloads
	                                 || blueprint_any_missing(blueprint)
	                                 || blueprint_any_rom_missing(blueprint);
	if (new_depot_query_needed)
		trigger_depot_query();

	if (popup_watches_downloads)
		_deploy.update_installation();

	bool const installation_complete =
		!update_state.attribute_value("progress", false);

	if (installation_complete) {
		_deploy.reattempt_after_installation();
	}
}


void Sculpt::Main::_handle_runtime_state()
{
	_runtime_state_rom.update();

	Xml_node state = _runtime_state_rom.xml();

	_runtime_state.update_from_state_report(state);

	bool reconfigure_runtime = false;

	/* check for completed storage operations */
	_storage._storage_devices.for_each([&] (Storage_device &device) {

		device.for_each_partition([&] (Partition &partition) {

			Storage_target const target { device.label, partition.number };

			if (partition.check_in_progress) {
				String<64> name(target.label(), ".fsck.ext2");
				Child_exit_state exit_state(state, name);

				if (exit_state.exited) {
					if (exit_state.code != 0)
						error("file-system check failed");
					if (exit_state.code == 0)
						log("file-system check succeeded");

					partition.check_in_progress = 0;
					reconfigure_runtime = true;
					_storage.dialog.reset_operation();
				}
			}

			if (partition.format_in_progress) {
				String<64> name(target.label(), ".mkfs.ext2");
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
				}
			}

		}); /* for each partition */

		/* respond to completion of GPT relabeling */
		if (device.relabel_in_progress()) {
			Child_exit_state exit_state(state, device.relabel_start_name());
			if (exit_state.exited) {
				device.rediscover();
				reconfigure_runtime = true;
				_storage.dialog.reset_operation();
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
			}
		}

	}); /* for each device */

	/* remove prepare subsystem when finished */
	{
		Child_exit_state exit_state(state, "prepare");
		if (exit_state.exited) {
			_prepare_completed = _prepare_version;

			/* trigger deployment */
			_deploy.handle_deploy();

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

		/* use binary OR (|), not logical OR (||), to always execute all elements */
		if (_storage._ram_fs_state.apply_child_state_report(child)
		  | _deploy.cached_depot_rom_state.apply_child_state_report(child)
		  | _deploy.uncached_depot_rom_state.apply_child_state_report(child)
		  | _runtime_view_state.apply_child_state_report(child)) {

			reconfigure_runtime = true;
			generate_dialog();
		}
	});

	/*
	 * Re-attempt NIC-router configuration as the uplink may have become
	 * available in the meantime.
	 */
	_network.reattempt_nic_router_config();

	if (_deploy.update_child_conditions()) {
		reconfigure_runtime = true;
		generate_dialog();
	}

	if (reconfigure_runtime)
		generate_runtime_config();
}


void Sculpt::Main::_generate_runtime_config(Xml_generator &xml) const
{
	xml.attribute("verbose", "yes");

	xml.node("report", [&] () {
		xml.attribute("init_ram",   "yes");
		xml.attribute("init_caps",  "yes");
		xml.attribute("child_ram",  "yes");
		xml.attribute("child_caps", "yes");
		xml.attribute("delay_ms",   4*500);
		xml.attribute("buffer",     "64K");
	});

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
		gen_parent_service<Nitpicker::Session>(xml);
		gen_parent_service<Rtc::Session>(xml);
		gen_parent_service<Trace::Session>(xml);
		gen_parent_service<Io_mem_session>(xml);
		gen_parent_service<Io_port_session>(xml);
		gen_parent_service<Irq_session>(xml);
	});

	xml.node("start", [&] () {
		gen_runtime_view_start_content(xml, _runtime_view_state, _gui.font_size()); });

	_storage.gen_runtime_start_nodes(xml);

	/*
	 * Load configuration and update depot config on the sculpt partition
	 */
	if (_storage._sculpt_partition.valid() && _prepare_in_progress())
		xml.node("start", [&] () {
			gen_prepare_start_content(xml, _prepare_version); });

	if (_storage.any_file_system_inspected())
		gen_file_browser(xml, _storage._storage_devices, _storage._ram_fs_state,
		                 _storage._file_browser_version);

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

		_deploy.gen_runtime_start_nodes(xml);
	}
}


void Component::construct(Genode::Env &env)
{
	static Sculpt::Main main(env);
}

