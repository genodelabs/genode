/*
 * \brief  Widget for configuring a new component deployed from a depot package
 * \author Norman Feske
 * \date   2023-03-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__COMPONENT_ADD_WIDGET_H_
#define _VIEW__COMPONENT_ADD_WIDGET_H_

#include <model/capacity.h>
#include <view/index_menu_widget.h>
#include <view/pd_route_widget.h>
#include <view/resource_widget.h>
#include <view/debug_widget.h>
#include <view/pd_route_widget.h>
#include <view/component_info_widget.h>

namespace Sculpt { struct Component_add_widget; }


struct Sculpt::Component_add_widget : Widget<Vbox>
{
	using Name   = Start_name;
	using Action = Component::Construction_action;

	Runtime_config const &_runtime_config;

	using Route_entry   = Hosted<Vbox, Frame, Vbox, Menu_entry>;
	using Service_entry = Hosted<Vbox, Frame, Vbox, Menu_entry>;

	Hosted<Vbox, Index_menu_widget::Sub_menu_title> _back      { Id { "back" } };
	Hosted<Vbox, Deferred_action_button>            _launch    { Id { "Add component" } };
	Hosted<Vbox, Frame, Vbox, Resource_widget>      _resources { Id { "resources" } };
	Hosted<Vbox, Frame, Pd_route_widget>            _pd_route  { Id { "pd_route" }, _runtime_config };
	Hosted<Vbox, Frame, Debug_widget>               _debug     { Id { "debug" } };

	Id _selected_route { };

	void _apply_to_selected_route(Action &action, auto const &fn)
	{
		unsigned count = 0;
		action.apply_to_construction([&] (Component &component) {
			component.routes.for_each([&] (Route &route) {
				if (_route_selected(Route::Id(count++)))
					fn(route); }); });
	}

	bool _route_selected(Route::Id const &id) const
	{
		return _selected_route.valid() && id == _selected_route.value;
	}

	bool _resource_widget_selected() const
	{
		return _route_selected("resources");
	}

	void _view_pkg_elements(Scope<Vbox> &s, Component const &component) const
	{
		using Info = Component::Info;

		s.widget(_back, Name { "Add ", Pretty(component.name) });

		s.widget(Hosted<Vbox, Component_info_widget> { Id { "info" } }, component);

		s.sub_scope<Annotation>(Info(Capacity{component.ram}, " ",
		                                      component.caps, " caps"));
		s.sub_scope<Vgap>();

		unsigned count = 0;
		component.routes.for_each([&] (Route const &route) {

			Id const id { Id::Value { count++ } };

			s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
				s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {

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
			s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
				s.widget(_pd_route, _selected_route, component); });

		s.sub_scope<Frame>(Id { "resources" }, [&] (Scope<Vbox, Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {

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

		s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
			s.widget(_debug, component); });

		/*
		 * Display "Add component" button once all routes are defined
		 */
		if (component.all_routes_defined())
			s.widget(_launch);
	}

	Component_add_widget(Runtime_config const &runtime_config)
	:
		_runtime_config(runtime_config)
	{ }

	void view(Scope<Vbox> &s, Component const &component) const
	{
		_view_pkg_elements(s, component);
	}

	void click(Clicked_at const &at, Action &action, auto const &leave_fn)
	{
		_back.propagate(at, [&] { leave_fn(); });

		_launch.propagate(at);

		if (at.matching_id<Vbox, Frame, Debug_widget>() == Id { "debug" })
			action.apply_to_construction([&] (Component &component) {
				_debug.propagate(at, component); });

		Id const route_id = at.matching_id<Vbox, Frame, Vbox, Menu_entry>();

		/* select route to present routing options */
		if (!_selected_route.valid() && route_id.valid())
			_selected_route = route_id;

		/*
		 * Route selected
		 */

		/* close selected route */
		if (route_id.value == "back") {
			_selected_route = { };

		} else if (_resource_widget_selected()) {

			bool const clicked_on_different_route = route_id.valid();
			if (clicked_on_different_route) {
				_selected_route = route_id;

			} else {

				action.apply_to_construction([&] (Component &component) {
					_resources.propagate(at, component); });
			}

		} else {

			bool clicked_on_selected_route = false;

			_apply_to_selected_route(action, [&] (Route &route) {

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

						_selected_route = { };

						clicked_on_selected_route = true;
					}
				});
			});

			if (_selected_route == _pd_route.id)
				action.apply_to_construction([&] (Component &component) {
					_pd_route.propagate(at, component); });

			/* select different route */
			if (!clicked_on_selected_route && route_id.valid())
				_selected_route = route_id;
		}
	}

	void clack(Clacked_at const &at, auto const &launch_fn)
	{
		_launch.propagate(at, launch_fn);
	}
};

#endif /* _VIEW__COMPONENT_ADD_WIDGET_H_ */
