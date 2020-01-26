/*
 * \brief  Popup dialog
 * \author Norman Feske
 * \date   2018-09-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <view/popup_dialog.h>
#include <string.h>

using namespace Sculpt;


void Popup_dialog::_gen_pkg_info(Xml_generator &xml,
                                 Component const &component) const
{
	if (component.info.length() > 1) {
		gen_named_node(xml, "label", "info", [&] () {
			xml.attribute("text", Component::Info(" ", component.info, " ")); });

		_gen_info_label(xml, "pad1", "");
	}
	_gen_info_label(xml, "path", component.path);
}


void Popup_dialog::_gen_pkg_elements(Xml_generator &xml,
                                     Component const &component) const
{
	typedef Component::Info Info;

	_gen_sub_menu_title(xml, "back", Menu::Name("Add ", Pretty(_construction_name)));

	_gen_pkg_info(xml, component);

	_gen_info_label(xml, "resources", Info(Capacity{component.ram}, " ",
	                                       component.caps, " caps"));
	_gen_info_label(xml, "pad2", "");

	unsigned cnt = 0;
	component.routes.for_each([&] (Route const &route) {

		Route::Id const id(cnt++);

		gen_named_node(xml, "frame", id, [&] () {

			xml.node("vbox", [&] () {

				bool const selected = _route_selected(id);
				bool const defined  = route.selected_service.constructed();

				if (!selected) {
					_gen_route_entry(xml, id,
					                 defined ? Info(route.selected_service->info)
					                         : Info(route),
					                 defined);
				}

				/*
				 * List of routing options
				 */
				if (selected) {
					_gen_route_entry(xml, "back", Info(route), true, "back");

					unsigned cnt = 0;
					_runtime_config.for_each_service([&] (Service const &service) {

						Hoverable_item::Id const id("service.", cnt++);

						bool const service_selected =
							route.selected_service.constructed() &&
							id == route.selected_service_id;

						if (service.type == route.required)
							_gen_route_entry(xml, id, service.info, service_selected);
					});
				}
			});
		});
	});

	/*
	 * Display "Add component" button once all routes are defined
	 */
	if (component.all_routes_defined()) {
		gen_named_node(xml, "button", "launch", [&] () {
			_action_item.gen_button_attr(xml, "launch");
			xml.node("label", [&] () {
				xml.attribute("text", "Add component"); });
		});
	}
}


void Popup_dialog::_gen_menu_elements(Xml_generator &xml) const
{
	/*
	 * Lauchers
	 */
	if (_state == TOP_LEVEL || _state < DEPOT_SHOWN) {
		_launchers.for_each([&] (Launchers::Info const &info) {

			/* allow each launcher to be used only once */
			if (_runtime_info.present_in_runtime(info.path))
				return;

			_gen_menu_entry(xml, info.path, Pretty(info.path), false);
		});

		_gen_menu_entry(xml, "depot", "Depot ...", false);
	}

	/*
	 * Depot users with an available index
	 */
	if (_state == DEPOT_SHOWN || _state == INDEX_REQUESTED) {
		_gen_sub_menu_title(xml, "back", "Depot");

		_scan_rom.xml().for_each_sub_node("user", [&] (Xml_node user) {

			User const name = user.attribute_value("name", User());
			bool const selected = (_selected_user == name);

			if (_index_avail(name))
				_gen_menu_entry(xml, name, User(name, " ..."), selected);
		});

		/*
		 * Depot selection menu item
		 */
		if (_state == DEPOT_SHOWN || _state == INDEX_REQUESTED) {

			if (_nic_ready())
				_gen_menu_entry(xml, "selection", "Selection ...", false);
		}
	}

	/*
	 * List of depot users for removing/adding indices
	 */
	if (_state == DEPOT_SELECTION) {
		_gen_sub_menu_title(xml, "back", "Selection");

		_scan_rom.xml().for_each_sub_node("user", [&] (Xml_node user) {

			User const name = user.attribute_value("name", User());
			bool const selected = _index_avail(name);

			String<32> const suffix = _download_queue.in_progress(_index_path(name))
			                        ? " fetch... " : " ";

			_gen_menu_entry(xml, name, User(name, suffix), selected, "checkbox");
		});
	}

	/*
	 * Title of index
	 */
	if (_state >= INDEX_SHOWN && _state < PKG_SHOWN) {
		Menu::Name title("Depot  ", _selected_user);
		if (_menu._level)
			title = Menu::Name(title, "  ", _menu, " ");

		_gen_sub_menu_title(xml, "back", title);
	}

	/*
	 * Index menu
	 */
	if (_state >= INDEX_SHOWN && _state < PKG_SHOWN) {

		unsigned cnt = 0;
		_for_each_menu_item([&] (Xml_node item) {

			Hoverable_item::Id const id(cnt);

			if (item.has_type("index")) {
				auto const name = item.attribute_value("name", Menu::Name());
				_gen_menu_entry(xml, id, Menu::Name(name, " ..."), false);
			}

			if (item.has_type("pkg")) {
				auto const path    = item.attribute_value("path", Depot::Archive::Path());
				auto const name    = Depot::Archive::name(path);
				auto const version = Depot::Archive::version(path);

				bool selected = false;

				bool const installing = _download_queue.in_progress(path);

				_construction_info.with_construction([&] (Component const &component) {

					if (component.path == path)
						selected = true;
				});

				String<100> const text(Pretty(name), " " "(", version, ")",
				                       installing ? " installing... " : "... ");

				_gen_menu_entry(xml, id, text, selected);

				if (selected && !installing) {

					_construction_info.with_construction([&] (Component const &component) {

						gen_named_node(xml, "float", "install", [&] () {

							gen_named_node(xml, "vbox", "vbox", [&] () {

								/*
								 * Package is installed but content is missing
								 *
								 * This can happen when the pkg's runtime is
								 * inconsistent with the content contained in
								 * the pkg's archives.
								 */
								if (!_pkg_missing && _pkg_rom_missing) {
									_gen_info_label(xml, "pad2", "");
									_gen_info_label(xml, "path", component.path);
									_gen_info_label(xml, "pad3", "");
									xml.node("label", [&] () {
										xml.attribute("text", "installed but incomplete"); });
								}

								/*
								 * Package is missing but can be installed
								 */
								else if (_pkg_missing && _nic_ready()) {

									_gen_pkg_info(xml, component);
									_gen_info_label(xml, "pad2", "");

									gen_named_node(xml, "float", "install", [&] () {
										xml.node("button", [&] () {
											_install_item.gen_button_attr(xml, "install");
											xml.node("label", [&] () {
												xml.attribute("text", "Install");
											});
										});
									});
								}

								/*
								 * Package is missing and we cannot do anything
								 * about it
								 */
								else if (_pkg_missing) {
									_gen_info_label(xml, "pad2", "");
									_gen_info_label(xml, "path", component.path);
									_gen_info_label(xml, "pad3", "");
									xml.node("label", [&] () {
										xml.attribute("text", "not installed"); });
								}

								_gen_info_label(xml, "pad4", "");
							});
						});
					});
				}
			}
			cnt++;
		});
	}

	/*
	 * Pkg configuration
	 */
	if (_state >= PKG_SHOWN)
		_construction_info.with_construction([&] (Component const &component) {
			_gen_pkg_elements(xml, component); });
}


void Popup_dialog::click(Action &action)
{
	Hoverable_item::Id const clicked = _item._hovered;

	_action_item .propose_activation_on_click();
	_install_item.propose_activation_on_click();

	Route::Id const clicked_route = _route_item._hovered;

	auto back_to_index = [&] ()
	{
		_state = INDEX_SHOWN;
		action.discard_construction();
		_selected_route.destruct();
	};

	if (_state == TOP_LEVEL) {

		if (clicked == "depot") {
			_state = DEPOT_REQUESTED;
			_depot_query.trigger_depot_query();
		} else {
			action.launch_global(clicked);
		}
	}

	else if (_state == DEPOT_SHOWN) {

		/* back to top-level menu */
		if (clicked == "back") {
			_state = TOP_LEVEL;

		} else if (clicked == "selection") {
			_state = DEPOT_SELECTION;

		} else {

			/* enter depot users menu */
			_selected_user = clicked;
			_state = INDEX_REQUESTED;
			_depot_query.trigger_depot_query();
		}
	}

	else if (_state == DEPOT_SELECTION) {

		/* back to depot users */
		if (clicked == "back") {
			_state = DEPOT_SHOWN;
		} else {

			if (!_index_avail(clicked))
				action.trigger_download(_index_path(clicked));
			else
				action.remove_index(clicked);
		}
	}

	else if (_state >= INDEX_SHOWN && _state < PKG_SHOWN) {

		/* back to depot users */
		if (_menu._level == 0 && clicked == "back") {
			_state = DEPOT_SHOWN;
			_selected_user = User();
		} else {

			/* go one menu up */
			if (clicked == "back") {
				_menu._selected[_menu._level] = Menu::Name();
				_menu._level--;
				action.discard_construction();
			} else {

				/* enter sub menu of index */
				if (_menu._level < Menu::MAX_LEVELS - 1) {

					unsigned cnt = 0;
					_for_each_menu_item([&] (Xml_node item) {

						if (clicked == Hoverable_item::Id(cnt)) {

							if (item.has_type("index")) {

								Menu::Name const name =
									item.attribute_value("name", Menu::Name());

								_menu._selected[_menu._level] = name;
								_menu._level++;

							} else if (item.has_type("pkg")) {

								auto path = item.attribute_value("path", Component::Path());
								auto info = item.attribute_value("info", Component::Info());

								_construction_name = action.new_construction(path, info);

								_state = PKG_REQUESTED;
								_pkg_missing = false;
								_depot_query.trigger_depot_query();
							}
						}
						cnt++;
					});
				}
			}
		}
	}

	else if (_state == PKG_SHOWN) {

		/* back to index */
		if (clicked == "back") {
			back_to_index();

		} else {

			/* select route to present routing options */
			if (clicked_route.valid()) {
				_state = ROUTE_SELECTED;
				_selected_route.construct(clicked_route);
			}
		}
	}

	else if (_state == ROUTE_SELECTED) {

		/* back to index */
		if (clicked == "back") {
			back_to_index();

		} else {

			/* close selected route */
			if (clicked_route == "back") {
				_state = PKG_SHOWN;
				_selected_route.destruct();

			} else {

				bool clicked_on_selected_route = false;

				_apply_to_selected_route(action, [&] (Route &route) {

					unsigned cnt = 0;
					_runtime_config.for_each_service([&] (Service const &service) {

						Hoverable_item::Id const id("service.", cnt++);

						if (clicked_route == id) {

							bool const clicked_service_already_selected =
								route.selected_service.constructed() &&
								id == route.selected_service_id;

							if (clicked_service_already_selected) {

								/* clear selection */
								route.selected_service.destruct();
								route.selected_service_id = Hoverable_item::Id();

							} else {

								/* select different service */
								route.selected_service.construct(service);
								route.selected_service_id = id;
							}

							_state = PKG_SHOWN;
							_selected_route.destruct();

							clicked_on_selected_route = true;
						}
					});
				});

				/* select different route */
				if (!clicked_on_selected_route && clicked_route.valid()) {
					_state = ROUTE_SELECTED;
					_selected_route.construct(clicked_route);
				}
			}
		}
	}
}
