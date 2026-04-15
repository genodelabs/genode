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
#include <view/pd_connect_widget.h>
#include <view/resource_widget.h>
#include <view/debug_widget.h>
#include <view/fs_connect_widget.h>
#include <view/component_info_widget.h>

namespace Sculpt { struct Component_add_widget; }


struct Sculpt::Component_add_widget : Widget<Vbox>
{
	using Name = Start_name;

	struct Action : virtual Component::Construction_action
	{
		virtual void query_directory(Dir_query::Query const &) = 0;
	};

	Runtime_config const &_runtime_config;
	Dir_query      const &_dir_query;

	using Conn_entry    = Hosted<Vbox, Frame, Vbox, Menu_entry>;
	using Service_entry = Hosted<Vbox, Frame, Vbox, Menu_entry>;

	Hosted<Vbox, Index_menu_widget::Sub_menu_title> _back      { Id { "back" } };
	Hosted<Vbox, Deferred_action_button>            _launch    { Id { "Add component" } };
	Hosted<Vbox, Frame, Vbox, Resource_widget>      _resources { Id { "resources" } };
	Hosted<Vbox, Frame, Pd_connect_widget>          _pd_conn   { Id { "pd_conn" }, _runtime_config };
	Hosted<Vbox, Frame, Debug_widget>               _debug     { Id { "debug" } };

	Id _selected_connection { };

	void _apply_to_selected_connection(Action &action, auto const &fn)
	{
		unsigned count = 0;
		action.apply_to_construction([&] (Component &component) {
			component.connect.for_each([&] (Connection &conn) {
				Connection::Id const id { count++ };
				if (_connection_selected(id))
					fn(conn, id); }); });
	}

	bool _connection_selected(Connection::Id const &id) const
	{
		return _selected_connection.valid() && id == _selected_connection.value;
	}

	bool _resource_widget_selected() const
	{
		return _connection_selected("resources");
	}

	bool _file_system_connection_selected(Action &action)
	{
		bool result = false;
		_apply_to_selected_connection(action, [&] (Connection const &conn, Connection::Id) {
			if (conn.required == Service::Type::FS)
				result = true; });
		return result;
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
		component.connect.for_each([&] (Connection const &conn) {

			Id const id { Id::Value { count++ } };

			/*
			 * Present directory selector for file-system connections
			 */
			if (conn.required == Service::Type::FS) {
				s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
					Hosted<Vbox, Frame, Fs_connect_widget> fs_connect_widget(id);
					s.widget(fs_connect_widget, _selected_connection, component,
					         conn, _runtime_config, _dir_query);
				});
				return;
			}

			s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
				s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {

					bool const selected = _connection_selected(id.value);
					bool const defined  = conn.selected_service.constructed();

					if (!selected) {
						Conn_entry entry { id };
						s.widget(entry, defined,
						         defined ? Info(conn.selected_service->info)
						                 : Info(conn));
					}

					/*
					 * List of routing options
					 */
					if (selected) {
						Conn_entry back { Id { "back" } };
						s.widget(back, true, Info(conn), "back");

						unsigned count = 0;
						_runtime_config.for_each_service([&] (Service const &service) {

							Id const service_id { Id::Value("service.", count++) };

							bool const service_selected =
								conn.selected_service.constructed() &&
								service_id.value == conn.selected_service_id;

							if (service.type == conn.required) {
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
				s.widget(_pd_conn, _selected_connection, component); });

		s.sub_scope<Frame>(Id { "resources" }, [&] (Scope<Vbox, Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {

				bool const selected = _connection_selected("resources");

				if (!selected) {
					Conn_entry entry { Id { "resources" } };
					s.widget(entry, false, "Resource assignment ...", "enter");
				}

				if (selected) {
					Conn_entry entry { Id { "back" } };
					s.widget(entry, true, "Resource assignment ...", "back");

					s.widget(_resources, component);
				}
			});
		});

		s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
			s.widget(_debug, component); });

		/*
		 * Display "Add component" button once all connections are defined
		 */
		if (component.all_connections_defined())
			s.widget(_launch);
	}

	Component_add_widget(Runtime_config const &runtime_config,
	                     Dir_query      const &dir_query)
	:
		_runtime_config(runtime_config), _dir_query(dir_query)
	{ }

	void reset() { _selected_connection = { }; }

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

		Id const conn_id = at.matching_id<Vbox, Frame, Vbox, Menu_entry>();

		/* select connection to present routing options */
		if (!_selected_connection.valid() && conn_id.valid())
			_selected_connection = conn_id;

		/*
		 * Connection selected
		 */

		/* close selected connection */
		if (conn_id.value == "back") {
			_selected_connection = { };

		} else if (_resource_widget_selected()) {

			bool const clicked_on_different_connection = conn_id.valid();
			if (clicked_on_different_connection) {
				_selected_connection = conn_id;

			} else {

				action.apply_to_construction([&] (Component &component) {
					_resources.propagate(at, component); });
			}

		} else if (_file_system_connection_selected(action)) {

			_apply_to_selected_connection(action, [&] (Connection &conn, Connection::Id id) {

				if (at.matching_id<Vbox, Frame, Fs_connect_widget>() != Id { id }) {
					/* select different connection */
					if (conn_id.valid())
						_selected_connection = conn_id;
					return;
				}

				/* click inside the file-system connection widget */
				action.apply_to_construction([&] (Component &component) {

					Hosted<Vbox, Frame, Fs_connect_widget> fs_connect_widget(Id { id });

					Connection::Id const orig_selected_service = conn.selected_service_id;
					Path           const orig_selected_path    = conn.selected_path;

					fs_connect_widget.propagate(at, _runtime_config, _dir_query,
					                            component, conn);

					Dir_query::Query const query =
						Fs_connect_widget::browsed_path_query(component, conn);

					if (query != _dir_query._query)
						action.query_directory(query);

					bool const selection_changed = (orig_selected_service != conn.selected_service_id)
					                            || (orig_selected_path    != conn.selected_path);

					if (selection_changed) /* close options on selection or deselection */
						_selected_connection = { };
				});
			});

		} else {

			bool clicked_on_selected_connection = false;

			_apply_to_selected_connection(action, [&] (Connection &conn, Connection::Id) {

				unsigned count = 0;
				_runtime_config.for_each_service([&] (Service const &service) {

					Id const id { Id::Value("service.", count++) };

					if (conn_id == id) {

						bool const clicked_service_already_selected =
							conn.selected_service.constructed() &&
							id.value == conn.selected_service_id;

						if (clicked_service_already_selected) {

							/* clear selection */
							conn.selected_service.destruct();
							conn.selected_service_id = { };

						} else {

							/* select different service */
							conn.selected_service.construct(service);
							conn.selected_service_id = id.value;
						}

						_selected_connection = { };

						clicked_on_selected_connection = true;
					}
				});
			});

			if (_selected_connection == _pd_conn.id)
				action.apply_to_construction([&] (Component &component) {
					_pd_conn.propagate(at, component); });

			/* select different connection */
			if (!clicked_on_selected_connection && conn_id.valid())
				_selected_connection = conn_id;
		}
	}

	void clack(Clacked_at const &at, auto const &launch_fn)
	{
		_launch.propagate(at, launch_fn);
	}
};

#endif /* _VIEW__COMPONENT_ADD_WIDGET_H_ */
