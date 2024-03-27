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


static Dialog::Id launcher_id(unsigned n) { return { Dialog::Id::Value("launcher ", n) }; }
static Dialog::Id user_id    (unsigned n) { return { Dialog::Id::Value("user ",     n) }; }


static void view_component_info(auto &s, Component const &component)
{
	if (component.info.length() > 1) {
		s.named_sub_node("label", "info", [&] {
			s.attribute("text", Component::Info(" ", component.info, " ")); });

		s.template sub_scope<Annotation>("");
	}
	s.template sub_scope<Annotation>(component.path);
}


void Popup_dialog::_view_pkg_elements(Scope<Frame, Vbox> &s,
                                      Component const &component) const
{
	using Info = Component::Info;

	s.widget(_back, Index_menu::Name("Add ", Pretty(_construction_name)));

	view_component_info(s, component);

	s.sub_scope<Annotation>(Info(Capacity{component.ram}, " ",
	                                      component.caps, " caps"));
	s.sub_scope<Vgap>();

	unsigned count = 0;
	component.routes.for_each([&] (Route const &route) {

		Id const id { Id::Value { count++ } };

		s.sub_scope<Frame>([&] (Scope<Frame, Vbox, Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox, Frame, Vbox> &s) {

				bool const selected = _route_selected(id.value);
				bool const defined  = route.selected_service.constructed();

				if (!selected) {
					Route_entry entry { id };
					s.widget(entry, defined,
					         defined ? Info(route.selected_service->info)
					                 : Info(route));
				}

				/*
				 * List of routing options
				 */
				if (selected) {
					Route_entry back { Id { "back" } };
					s.widget(back, true, Info(route), "back");

					unsigned count = 0;
					_runtime_config.for_each_service([&] (Service const &service) {

						Id const service_id { Id::Value("service.", count++) };

						bool const service_selected =
							route.selected_service.constructed() &&
							service_id.value == route.selected_service_id;

						if (service.type == route.required) {
							Service_entry entry { service_id };
							s.widget(entry, service_selected, service.info);
						}
					});
				}
			});
		});
	});

	/* don't show the PD menu if only the system PD service is available */
	if (_runtime_config.num_service_options(Service::Type::PD) > 1)
		s.sub_scope<Frame>([&] (Scope<Frame, Vbox, Frame> &s) {
			s.widget(_pd_route, _selected_route, component); });

	s.sub_scope<Frame>(Id { "resources" }, [&] (Scope<Frame, Vbox, Frame> &s) {
		s.sub_scope<Vbox>([&] (Scope<Frame, Vbox, Frame, Vbox> &s) {

			bool const selected = _route_selected("resources");

			if (!selected) {
				Route_entry entry { Id { "resources" } };
				s.widget(entry, false, "Resource assignment ...", "enter");
			}

			if (selected) {
				Route_entry entry { Id { "back" } };
				s.widget(entry, true, "Resource assignment ...", "back");

				s.widget(_resources, component);
			}
		});
	});

	s.sub_scope<Frame>([&] (Scope<Frame, Vbox, Frame> &s) {
		s.widget(_debug, component); });

	/*
	 * Display "Add component" button once all routes are defined
	 */
	if (component.all_routes_defined())
		s.widget(_launch);
}


void Popup_dialog::_view_menu_elements(Scope<Frame, Vbox> &s, Xml_node const &depot_users) const
{
	/*
	 * Lauchers
	 */
	if (_state == TOP_LEVEL || _state < DEPOT_SHOWN) {
		unsigned count = 0;
		for_each_viewed_launcher([&] (Launchers::Info const &info) {
			Hosted<Frame, Vbox, Menu_entry> menu_entry { launcher_id(count++) };
			s.widget(menu_entry, false, String<100>(Pretty(info.path)));
		});

		Hosted<Frame, Vbox, Menu_entry> depot_menu_entry { Id { "depot" } };
		s.widget(depot_menu_entry, false, "Depot ...");
	}

	/*
	 * Depot users with an available index
	 */
	if (_state == DEPOT_SHOWN || _state == INDEX_REQUESTED) {
		s.widget(_back, "Depot");

		unsigned count = 0;
		depot_users.for_each_sub_node("user", [&] (Xml_node user) {

			User const name     = user.attribute_value("name", User());
			bool const selected = (_selected_user == name);
			Id   const id       = user_id(count++);

			if (_index_avail(name)) {
				Hosted<Frame, Vbox, Menu_entry> menu_entry { id };
				s.widget(menu_entry, selected, User(name, " ..."));
			}
		});

		/*
		 * Depot selection menu item
		 */
		if (_nic_ready()) {
			Hosted<Frame, Vbox, Menu_entry> menu_entry { Id { "selection" } };
			s.widget(menu_entry, false, "Selection ...");
		}
	}

	/*
	 * List of depot users for removing/adding indices
	 */
	if (_state == DEPOT_SELECTION) {
		s.widget(_back, "Selection");

		unsigned count = 0;
		depot_users.for_each_sub_node("user", [&] (Xml_node user) {

			User const name = user.attribute_value("name", User());
			bool const selected = _index_avail(name);
			Id   const id       = user_id(count++);

			String<32> const suffix = _download_queue.in_progress(_index_path(name))
			                        ? " fetch... " : " ";

			Hosted<Frame, Vbox, Menu_entry> user_entry { id };
			s.widget(user_entry, selected, User(name, suffix), "checkbox");
		});
	}

	/*
	 * Title of index
	 */
	if (_state >= INDEX_SHOWN && _state < PKG_SHOWN) {
		Index_menu::Name title("Depot  ", _selected_user);
		if (_menu._level)
			title = Index_menu::Name(title, "  ", _menu, " ");

		s.widget(_back, title);
	}

	/*
	 * Index menu
	 */
	if (_state >= INDEX_SHOWN && _state < PKG_SHOWN) {

		unsigned count = 0;
		_for_each_menu_item([&] (Xml_node item) {

			Id const id { Id::Value(count) };

			if (item.has_type("index")) {
				auto const name = item.attribute_value("name", Index_menu::Name());
				Hosted<Frame, Vbox, Menu_entry> entry { id };
				s.widget(entry, false, Index_menu::Name(name, " ..."));
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

				Hosted<Frame, Vbox, Menu_entry> entry { id };
				s.widget(entry, selected, text);

				if (selected && !installing) {

					_construction_info.with_construction([&] (Component const &component) {

						s.sub_scope<Float>(Id { "install" }, [&] (Scope<Frame, Vbox, Float> &s) {
							s.sub_scope<Vbox>([&] (Scope<Frame, Vbox, Float, Vbox> &s) {

								/*
								 * Package is installed but content is missing
								 *
								 * This can happen when the pkg's runtime is
								 * inconsistent with the content contained in
								 * the pkg's archives.
								 */
								if (_blueprint_info.incomplete()) {
									s.sub_scope<Vgap>();
									s.sub_scope<Annotation>(component.path);
									s.sub_scope<Vgap>();
									s.sub_scope<Label>("installed but incomplete");

									if (_nic_ready()) {
										s.sub_scope<Vgap>();
										s.sub_scope<Float>([&] (Scope<Frame, Vbox, Float, Vbox, Float> &s) {
											s.widget(_install, [&] (Scope<Button> &s) {
												s.sub_scope<Label>("Reattempt Install"); }); });
									}
									s.sub_scope<Vgap>();
								}

								/*
								 * Package is missing but can be installed
								 */
								else if (_blueprint_info.uninstalled() && _nic_ready()) {

									view_component_info(s, component);

									s.sub_scope<Vgap>();
									s.sub_scope<Float>([&] (Scope<Frame, Vbox, Float, Vbox, Float> &s) {
										s.widget(_install, [&] (Scope<Button> &s) {
											s.sub_scope<Label>("Install"); }); });
									s.sub_scope<Vgap>();
								}

								/*
								 * Package is missing and we cannot do anything
								 * about it
								 */
								else if (_blueprint_info.uninstalled()) {
									s.sub_scope<Vgap>();
									s.sub_scope<Annotation>(component.path);
									s.sub_scope<Vgap>();
									s.sub_scope<Label>("not installed");
									s.sub_scope<Vgap>();
								}
							});
						});
					});
				}
			}
			count++;
		});
	}

	/*
	 * Pkg configuration
	 */
	if (_state >= PKG_SHOWN)
		_construction_info.with_construction([&] (Component const &component) {
			_view_pkg_elements(s, component); });
}


void Popup_dialog::click(Clicked_at const &at)
{
	bool clicked_on_back_button = false;

	_back.propagate(at, [&] {
		clicked_on_back_button = true;

		switch (_state) {
		case TOP_LEVEL:                             break;
		case DEPOT_REQUESTED:                       break;
		case DEPOT_SHOWN:     _state = TOP_LEVEL;   break;
		case DEPOT_SELECTION: _state = DEPOT_SHOWN; break;
		case INDEX_REQUESTED:                       break;

		case INDEX_SHOWN:
		case PKG_REQUESTED:
			if (_menu._level > 0) {
				/* go one menu up */
				_menu._selected[_menu._level] = Index_menu::Name();
				_menu._level--;
				_action.discard_construction();
			} else {
				_state = DEPOT_SHOWN;
				_selected_user = User();
			}
			break;

		case PKG_SHOWN:
		case ROUTE_SELECTED:
			_state = INDEX_SHOWN;
			_action.discard_construction();
			_selected_route = { };
			break;
		}
	});

	if (clicked_on_back_button)
		return;

	_launch.propagate(at);
	_install.propagate(at);

	Id const route_id = at.matching_id<Frame, Vbox, Frame, Vbox, Menu_entry>();

	auto with_matching_user = [&] (Id const &id, auto const &fn)
	{
		unsigned count = 0;
		_depot_users.with_xml([&] (Xml_node const &users) {
			users.for_each_sub_node("user", [&] (Xml_node const &user) {
				if (id == user_id(count++))
					fn(user.attribute_value("name", User())); }); });
	};

	State const orig_state = _state;

	if (orig_state == TOP_LEVEL) {

		Id const id = at.matching_id<Frame, Vbox, Menu_entry>();

		if (id.value == "depot") {
			_state = DEPOT_REQUESTED;
			_depot_query.trigger_depot_query();

		} else {

			unsigned count = 0;

			for_each_viewed_launcher([&] (Launchers::Info const &info) {
				if (id == launcher_id(count++))
					_action.launch_global(info.path); });
		}
	}

	else if (orig_state == DEPOT_SHOWN) {

		Id const id = at.matching_id<Frame, Vbox, Menu_entry>();

		if (id.value == "selection")
			_state = DEPOT_SELECTION;

		/* enter depot users menu */
		with_matching_user(id, [&] (User const &user) {
			_selected_user = user;
			_state         = INDEX_REQUESTED;
			_depot_query.trigger_depot_query();
		});
	}

	else if (orig_state == DEPOT_SELECTION) {

		Id const id = at.matching_id<Frame, Vbox, Menu_entry>();

		with_matching_user(id, [&] (User const &user) {

			if (!_index_avail(user))
				_action.trigger_download(_index_path(user), Verify{true});
			else
				_action.remove_index(user);
		});
	}

	else if (orig_state >= INDEX_SHOWN && orig_state < PKG_SHOWN) {

		Id const id = at.matching_id<Frame, Vbox, Menu_entry>();

		/* enter sub menu of index */
		if (_menu._level < Index_menu::MAX_LEVELS - 1) {

			unsigned count = 0;
			_for_each_menu_item([&] (Xml_node item) {

				if (id.value == Id::Value(count)) {

					if (item.has_type("index")) {

						Index_menu::Name const name =
							item.attribute_value("name", Index_menu::Name());

						_menu._selected[_menu._level] = name;
						_menu._level++;

					} else if (item.has_type("pkg")) {

						auto path = item.attribute_value("path", Component::Path());
						auto info = item.attribute_value("info", Component::Info());

						_construction_name =
							_action.new_construction(path, Verify{true}, info);

						_state = PKG_REQUESTED;
						_depot_query.trigger_depot_query();
					}
				}
				count++;
			});
		}
	}

	else if (orig_state == PKG_SHOWN) {

		/* select route to present routing options */
		if (route_id.valid()) {
			_state = ROUTE_SELECTED;
			_selected_route = route_id;
		}
	}

	else if (orig_state == ROUTE_SELECTED) {

		/* close selected route */
		if (route_id.value == "back") {
			_state = PKG_SHOWN;
			_selected_route = { };

		} else if (_resource_dialog_selected()) {

			bool const clicked_on_different_route = route_id.valid();
			if (clicked_on_different_route) {
				_selected_route = route_id;

			} else {

				_action.apply_to_construction([&] (Component &component) {
					_resources.propagate(at, component); });
			}

		} else {

			bool clicked_on_selected_route = false;

			_apply_to_selected_route(_action, [&] (Route &route) {

				unsigned count = 0;
				_runtime_config.for_each_service([&] (Service const &service) {

					Id const id { Id::Value("service.", count++) };

					if (route_id == id) {

						bool const clicked_service_already_selected =
							route.selected_service.constructed() &&
							id.value == route.selected_service_id;

						if (clicked_service_already_selected) {

							/* clear selection */
							route.selected_service.destruct();
							route.selected_service_id = { };

						} else {

							/* select different service */
							route.selected_service.construct(service);
							route.selected_service_id = id.value;
						}

						_state = PKG_SHOWN;
						_selected_route = { };

						clicked_on_selected_route = true;
					}
				});
			});

			if (_selected_route == _pd_route.id)
				_action.apply_to_construction([&] (Component &component) {
					_pd_route.propagate(at, component); });

			/* select different route */
			if (!clicked_on_selected_route && route_id.valid()) {
				_state = ROUTE_SELECTED;
				_selected_route = route_id;
			}
		}
	}

	if (orig_state == PKG_SHOWN || orig_state == ROUTE_SELECTED)
		_action.apply_to_construction([&] (Component &component) {
			_debug.propagate(at, component); });
}
