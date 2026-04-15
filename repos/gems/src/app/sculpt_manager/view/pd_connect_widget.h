/*
 * \brief  PD/CPU connection assignment widget
 * \author Norman Feske
 * \date   2023-10-30
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__PD_CONNECT_WIDGET_H_
#define _VIEW__PD_CONNECT_WIDGET_H_

/* local includes */
#include <model/component.h>
#include <model/runtime_config.h>
#include <view/dialog.h>

namespace Sculpt { struct Pd_connect_widget; }


struct Sculpt::Pd_connect_widget : Widget<Vbox>
{
	Runtime_config const &_runtime_config;

	Pd_connect_widget(Runtime_config const &runtime_config)
	:
		_runtime_config(runtime_config)
	{ }

	void view(Scope<Vbox> &s, Id const &selected_connection, Component const &component) const
	{
		using Conn_entry    = Hosted<Vbox, Menu_entry>;
		using Service_entry = Hosted<Vbox, Menu_entry>;
		using Info          = Component::Info;

		Id const pd_conn_id = s.id;

		bool const selected = (selected_connection == pd_conn_id);

		if (!selected) {
			bool const defined = component.pd_connection.selected_service.constructed();

			Conn_entry entry { pd_conn_id };
			s.widget(entry, defined,
			         defined ? Info(component.pd_connection.selected_service->info)
			                 : Info("PD"));
			return;
		}

		/*
		 * List of connection options
		 */
		Conn_entry back { Id { "back" } };
		s.widget(back, true, "PD", "back");

		unsigned count = 0;
		_runtime_config.for_each_service([&] (Service const &service) {

			Id const service_id { Id::Value("service.", count++) };

			bool const service_selected =
				component.pd_connection.selected_service.constructed() &&
				service_id.value == component.pd_connection.selected_service_id;

			if (service.type == Service::Type::PD) {
				Service_entry entry { service_id };
				s.widget(entry, service_selected, service.info);
			}
		});
	}

	void click(Clicked_at const &at, Component &component)
	{
		Id const id = at.matching_id<Vbox, Menu_entry>();

		unsigned count = 0;
		_runtime_config.for_each_service([&] (Service const &service) {

			Id const service_id { Id::Value("service.", count++) };

			if (id == service_id) {
				Connection &conn = component.pd_connection;
				if (conn.selected_service_id == service_id.value) {
					conn.selected_service.destruct();
					conn.selected_service_id = { };
				} else {
					conn.selected_service.construct(service);
					conn.selected_service_id = service_id.value;
				}
			}
		});
	}
};

#endif /* _VIEW__PD_CONNECT_WIDGET_H_ */
