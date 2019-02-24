/*
 * \brief  Representation of a component
 * \author Norman Feske
 * \date   2019-02-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__COMPONENT_H_
#define _MODEL__COMPONENT_H_

#include <types.h>
#include <model/route.h>

namespace Sculpt { struct Component; }


struct Sculpt::Component : Noncopyable
{
	Route::Update_policy _route_update_policy;

	typedef Depot::Archive::Path Path;
	typedef Depot::Archive::Name Name;
	typedef String<100>          Info;
	typedef Start_name           Service;

	/* defined at construction time */
	Path const path;
	Info const info;

	/* defined when blueprint arrives */
	uint64_t ram  { };
	size_t   caps { };

	bool blueprint_known = false;

	List_model<Route> routes { };

	Component(Allocator &alloc, Path const &path, Info const &info)
	: _route_update_policy(alloc), path(path), info(info) { }

	~Component()
	{
		routes.update_from_xml(_route_update_policy, Xml_node("<empty/>"));
	}

	void try_apply_blueprint(Xml_node blueprint)
	{
		blueprint.for_each_sub_node("pkg", [&] (Xml_node pkg) {

			if (path != pkg.attribute_value("path", Path()))
				return;

			pkg.with_sub_node("runtime", [&] (Xml_node runtime) {

				ram  = runtime.attribute_value("ram", Number_of_bytes());
				caps = runtime.attribute_value("caps", 0UL);

				runtime.with_sub_node("requires", [&] (Xml_node requires) {
					routes.update_from_xml(_route_update_policy, requires); });
			});

			blueprint_known = true;
		});
	}

	bool all_routes_defined() const
	{
		bool result = true;
		routes.for_each([&] (Route const &route) {
			if (!route.selected_service.constructed())
				result = false; });

		return result;
	}
};

#endif /* _MODEL__COMPONENT_H_ */
