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
#include <depot_query.h>

namespace Sculpt { struct Component; }


struct Sculpt::Component : Noncopyable
{
	typedef Depot::Archive::Path Path;
	typedef Depot::Archive::Name Name;
	typedef String<100>          Info;
	typedef Start_name           Service;

	Allocator &_alloc;

	/* defined at construction time */
	Name   const name;
	Path   const path;
	Verify const verify;
	Info   const info;


	/* defined when blueprint arrives */
	uint64_t ram  { };
	size_t   caps { };

	Affinity::Space const affinity_space;
	Affinity::Location    affinity_location { 0, 0,
	                                          affinity_space.width(),
	                                          affinity_space.height() };
	Priority priority = Priority::DEFAULT;

	bool monitor        { false };
	bool wait           { false };
	bool wx             { false };
	bool system_control { false };

	struct Blueprint_info
	{
		bool known;
		bool pkg_avail;
		bool content_complete;

		bool uninstalled()     const { return known && !pkg_avail; }
		bool ready_to_deploy() const { return known && pkg_avail &&  content_complete; }
		bool incomplete()      const { return known && pkg_avail && !content_complete; }
	};

	Blueprint_info blueprint_info { };

	List_model<Route> routes   { };
	Route             pd_route { "<pd/>" };

	void _update_routes_from_xml(Xml_node const &node)
	{
		routes.update_from_xml(node,

			/* create */
			[&] (Xml_node const &route) -> Route & {
				return *new (_alloc) Route(route); },

			/* destroy */
			[&] (Route &e) { destroy(_alloc, &e); },

			/* update */
			[&] (Route &, Xml_node) { }
		);
	}

	struct Construction_info : Interface
	{
		struct With : Interface { virtual void with(Component const &) const = 0; };

		virtual void _with_construction(With const &) const = 0;

		template <typename FN>
		void with_construction(FN const &fn) const
		{
			struct _With : With {
				FN const &_fn;
				_With(FN const &fn) : _fn(fn) { }
				void with(Component const &c) const override { _fn(c); }
			};
			_with_construction(_With(fn));
		}
	};

	struct Construction_action : Interface
	{
		virtual void new_construction(Path const &pkg, Verify, Info const &) = 0;

		struct Apply_to : Interface { virtual void apply_to(Component &) = 0; };

		virtual void _apply_to_construction(Apply_to &) = 0;

		template <typename FN>
		void apply_to_construction(FN const &fn)
		{
			struct _Apply_to : Apply_to {
				FN const &_fn;
				_Apply_to(FN const &fn) : _fn(fn) { }
				void apply_to(Component &c) override { _fn(c); }
			} apply_fn(fn);

			_apply_to_construction(apply_fn);
		}

		virtual void discard_construction() = 0;
		virtual void launch_construction() = 0;
		virtual void trigger_pkg_download() = 0;
	};

	Component(Allocator &alloc, Name const &name, Path const &path,
	          Verify verify, Info const &info, Affinity::Space const space)
	:
		_alloc(alloc), name(name), path(path), verify(verify), info(info),
		affinity_space(space)
	{ }

	~Component()
	{
		_update_routes_from_xml(Xml_node("<empty/>"));
	}

	void try_apply_blueprint(Xml_node blueprint)
	{
		blueprint_info = { };

		blueprint.for_each_sub_node([&] (Xml_node pkg) {

			if (path != pkg.attribute_value("path", Path()))
				return;

			if (pkg.has_type("missing")) {
				blueprint_info.known = true;
				return;
			}

			pkg.with_optional_sub_node("runtime", [&] (Xml_node runtime) {

				ram  = runtime.attribute_value("ram", Number_of_bytes());
				caps = runtime.attribute_value("caps", 0UL);

				runtime.with_optional_sub_node("requires", [&] (Xml_node const &req) {
					_update_routes_from_xml(req); });
			});

			blueprint_info = {
				.known            = true,
				.pkg_avail        = !blueprint_missing(blueprint, path),
				.content_complete = !blueprint_rom_missing(blueprint, path)
			};
		});
	}

	void gen_priority(Xml_generator &xml) const
	{
		xml.attribute("priority", (int)priority);
	}

	void gen_system_control(Xml_generator &xml) const
	{
		if (system_control)
			xml.attribute("managing_system", "yes");
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

	void gen_monitor(Xml_generator &xml) const
	{
		if (monitor)
			xml.node("monitor", [&] () {
				xml.attribute("wait", wait ? "yes" : "no");
				xml.attribute("wx", wx ? "yes" : "no");
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
