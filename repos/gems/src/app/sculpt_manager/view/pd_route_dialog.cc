/*
 * \brief  PD/CPU route assignment dialog
 * \author Alexander Boettcher
 * \date   2021-02-26
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <view/pd_route_dialog.h>

using namespace Sculpt;

void Pd_route_dialog::generate(Xml_generator &xml) const
{
	/* find out number of available PD services */
	unsigned count_pd_services = 0;
	_runtime_config.for_each_service([&] (Service const &service) {

		if (service.type == Service::Type::PD)
			count_pd_services ++;
	});

	/* don't show the PD menu if just the system PD service is available */
	if (count_pd_services <= 1)
		return;

	{
		Route::Id const pd_id("pd_route");

		gen_named_node(xml, "frame", pd_id, [&] () {

			xml.node("vbox", [&] () {

				bool const defined  = _route.selected_service.constructed();

				if (!_menu_selected) {
					_gen_route_entry(xml, pd_id,
					                 defined ? Component::Info(_route.selected_service->info)
					                         : Component::Info(_route),
					                 defined);
				}

				/*
				 * List of routing options
				 */
				if (_menu_selected) {
					_gen_route_entry(xml, "back", Component::Info(_route), true, "back");

					unsigned cnt = 0;
					_runtime_config.for_each_service([&] (Service const &service) {

						Hoverable_item::Id const id("service.", cnt++);

						bool const service_selected =
							_route.selected_service.constructed() &&
							id == _route.selected_service_id;

						if (service.type == _route.required)
							_gen_route_entry(xml, id, service.info, service_selected);
					});
				}
			});
		});
	}
}

void Pd_route_dialog::click(Component &component)
{
	if (_route_item.hovered("pd_route")) {
		_menu_selected = true;
	}

	if (!_menu_selected)
		return;

	unsigned cnt = 0;
	_runtime_config.for_each_service([&] (Service const &service) {

		Hoverable_item::Id const id("service.", cnt++);

		if (!_route_item.hovered(id))
			return;

		if (_route.selected_service.constructed()) {
			if (component.pd_route.selected_service.constructed())
				component.pd_route.selected_service.destruct();

			_route.selected_service.destruct();
			if (_route_item.hovered(_route.selected_service_id)) {
				_route.selected_service_id = Hoverable_item::Id();
				return;
			}
		}

		component.pd_route.selected_service.construct(service);

		_route.selected_service.construct(service);
		_route.selected_service_id = id;

		_menu_selected = false;
	});
}
