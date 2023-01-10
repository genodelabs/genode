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
#include <depot/archive.h>

namespace Sculpt { struct Component; }


struct Sculpt::Component : Noncopyable
{
	typedef Depot::Archive::Path Path;
	typedef Depot::Archive::Name Name;
	typedef String<100>          Info;
	typedef Start_name           Service;

	Allocator &_alloc;

	/* defined at construction time */
	Path const path;
	Info const info;

	/* defined when blueprint arrives */
	uint64_t ram  { };
	size_t   caps { };

	Affinity::Space const affinity_space;
	Affinity::Location    affinity_location { 0, 0,
	                                          affinity_space.width(),
	                                          affinity_space.height() };
	Priority priority = Priority::DEFAULT;

	bool blueprint_known = false;

	List_model<Route> routes   { };
	Route             pd_route { "<pd/>" };

	void _update_routes_from_xml(Xml_node const &node)
	{
		update_list_model_from_xml(routes, node,

			/* create */
			[&] (Xml_node const &route) -> Route & {
				return *new (_alloc) Route(route); },

			/* destroy */
			[&] (Route &e) { destroy(_alloc, &e); },

			/* update */
			[&] (Route &, Xml_node) { }
		);
	}

	Component(Allocator &alloc, Path const &path, Info const &info,
	          Affinity::Space const space)
	:
		_alloc(alloc), path(path), info(info), affinity_space(space)
	{ }

	~Component()
	{
		_update_routes_from_xml(Xml_node("<empty/>"));
	}

	void try_apply_blueprint(Xml_node blueprint)
	{
		blueprint.for_each_sub_node("pkg", [&] (Xml_node pkg) {

			if (path != pkg.attribute_value("path", Path()))
				return;

			pkg.with_optional_sub_node("runtime", [&] (Xml_node runtime) {

				ram  = runtime.attribute_value("ram", Number_of_bytes());
				caps = runtime.attribute_value("caps", 0UL);

				runtime.with_optional_sub_node("requires", [&] (Xml_node requires) {
					_update_routes_from_xml(requires); });
			});

			blueprint_known = true;
		});
	}

	void gen_priority(Xml_generator &xml) const
	{
		xml.attribute("priority", (int)priority);
	}

	void gen_affinity(Xml_generator &xml) const
	{
		bool const all_cpus = affinity_space.width()  == affinity_location.width()
		                   && affinity_space.height() == affinity_location.height();

		/* omit <affinity> node if all CPUs are used by the component */
		if (all_cpus)
			return;

		xml.node("affinity", [&] () {
			xml.attribute("xpos",   affinity_location.xpos());
			xml.attribute("ypos",   affinity_location.ypos());
			xml.attribute("width",  affinity_location.width());
			xml.attribute("height", affinity_location.height());
		});
	}

	void gen_pd_cpu_route(Xml_generator &xml) const
	{
		/* by default pd route goes to parent if nothing is specified */
		if (!pd_route.selected_service.constructed())
			return;

		/*
		 * Until PD & CPU gets merged, enforce on Sculpt that PD and CPU routes
		 * go to the same server.
		 */
		gen_named_node(xml, "service", Sculpt::Service::name_attr(pd_route.required), [&] () {
			pd_route.selected_service->gen_xml(xml); });
		gen_named_node(xml, "service", "CPU", [&] () {
			pd_route.selected_service->gen_xml(xml); });
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
