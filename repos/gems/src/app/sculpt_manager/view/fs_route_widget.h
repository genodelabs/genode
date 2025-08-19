/*
 * \brief  File-system route assignment widget
 * \author Norman Feske
 * \date   2025-04-19
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__FS_ROUTE_WIDGET_H_
#define _VIEW__FS_ROUTE_WIDGET_H_

/* local includes */
#include <model/component.h>
#include <model/dir_query.h>
#include <view/dialog.h>

namespace Sculpt { struct Fs_route_widget; }


struct Sculpt::Fs_route_widget : Widget<Vbox>
{
	using Identity = Dir_query::Identity;

	struct Left_title_above_annotation : Sub_scope
	{
		static void view_sub_scope(auto &s, auto const &title, auto const &annotation)
		{
			s.node("hbox", [&] {
				s.sub_node("label", [&] {
					s.g.node("text", [&] { s.g.append_quoted(" "); }); });
				s.sub_node("vbox", [&] {
					s.named_sub_node("float", "title", [&] {
						s.attribute("west", "yes");
						s.sub_node("label", [&] {
							s.g.node("text", [&] { s.g.append_quoted(title); }); }); });
					if (annotation.length() > 1)
						s.named_sub_node("float", "annotation", [&] {
							s.attribute("west", "yes");
							Annotation::sub_node(s, annotation); });
				});
			});
		}

		static void with_narrowed_at(auto const &, auto const &) { }
	};

	struct Folded_entry : Widget<Left_floating_hbox>
	{
		void view(Scope<Left_floating_hbox> &s, bool selected,
		          auto const &text, Path selected_path) const
		{
			bool const hovered = (s.hovered() && !s.dragged());

			Path const annotation = selected_path.length() > 1
			                      ? Path { selected_path, "/" } : Path { };

			s.sub_scope<Icon>("radio", Icon::Attr { .hovered  = hovered,
			                                        .selected = selected });
			s.sub_scope<Left_title_above_annotation>(Id {"label"}, text, annotation);
			s.sub_scope<Button_vgap>();
		}

		void click(Clicked_at const &, auto const &fn) const { fn(); }
	};

	struct Dir_entry : Widget<Left_floating_hbox>
	{
		struct Attr { unsigned level; bool selected, has_subdirs, expanded; };

		void view(Scope<Left_floating_hbox> &s, auto const &text, Attr attr) const
		{
			bool const radio_hovered = s.hovered<Float>(Id { "radio" })
			                        || s.hovered<Hbox> (Id { "label" });

			for (unsigned i = 0; i < attr.level; i++)
				s.sub_scope<Icon>("invisible", Icon::Attr { });

			s.sub_scope<Icon>(Id {"radio"}, "radio", Icon::Attr {
				.hovered  = radio_hovered && !s.dragged(),
				.selected = attr.selected });
			s.sub_scope<Hbox>(Id {"label"}, [&] (Scope<Left_floating_hbox, Hbox> &s) {
				s.sub_scope<Label>(String<100>(" ", text, " ")); });

			if (attr.has_subdirs)
				s.sub_scope<Icon>(Id { "detail" }, "detail", Icon::Attr {
					.hovered  = s.hovered<Float>(Id { "detail" }),
					.selected = attr.expanded });

			s.sub_scope<Button_vgap>();
		}

		void click(Clicked_at const &at, auto const &radio_fn, auto const &detail_fn) const
		{
			if (at.matches<Left_floating_hbox, Float>(Id { "radio" })
			 || at.matches<Left_floating_hbox, Hbox> (Id { "label" }))
				radio_fn();
			else if (at.matches<Left_floating_hbox, Float>(Id { "detail" }))
				detail_fn();
		}
	};

	using Fs_entry = Hosted<Vbox, Dir_entry>;

	static Path without_leading_slash(Path const &p)
	{
		return p.string()[0] == '/' ? Path { p.string() + 1 } : Path { };
	}

	static Path first_path_element(Path const &p)
	{
		return with_split(without_leading_slash(p), '/',
		                  [&] (Path const &head, Path const &) { return head; });
	}

	static Path with_leading_slash(Path const &p)
	{
		return p.length() > 1 ? Path { "/", p } : Path { };
	}

	static Path without_first_path_element(Path const &p)
	{
		return with_split(without_leading_slash(p), '/',
			[&] (Path const &, Path const &tail) {
				return with_leading_slash(tail); });
	}

	static Dir_query::Identity _identity(Component const &component,
	                                     Route const &route)
	{
		return { component.name, " -> ", route.required_label };
	}

	static void _for_each_browsed_leading_sub_path(Path const &p, auto const &fn)
	{
		Path leading { "/", first_path_element(p) };
		Path remaining = without_leading_slash(without_first_path_element(p));
		while (remaining.length() > 1)
			with_split(remaining, '/', [&] (Path const &head, Path const &tail) {
				leading   = { leading, "/", head };
				remaining = tail;
				Path const &const_leading = leading;
				fn(const_leading, head); });
	}

	static Dir_query::Query browsed_path_query(Component const &component, Route const &route)
	{
		return {
			.identity = _identity(component, route),
			.fs       =         first_path_element(route.browsed.path),
			.path     = without_first_path_element(route.browsed.path)
		};
	}

	static Id _path_elem_id(Route const &route, unsigned level)
	{
		return { { "l", level, ".", route.browsed.index_at_level(level) } };
	}

	void view(Scope<Vbox> &s, Id const &selected_route, Component const &component,
	          Route const &route, Runtime_config const &runtime_config,
	          Dir_query const &dir_query) const
	{
		using Info = Component::Info;

		Id const fs_route_id = s.id;

		bool const selected = (selected_route == fs_route_id);

		if (!selected) {
			bool const defined = route.selected_service.constructed();

			Hosted<Vbox, Folded_entry> entry { fs_route_id };
			s.widget(entry, defined,
			         defined ? Info(route.selected_service->info)
			                 : Info(route),
			         route.selected_path);
			return;
		}

		Hosted<Vbox, Menu_entry> back { Id { "back" } };
		s.widget(back, true, Info(route), "back");

		unsigned count = 0;
		runtime_config.for_each_service([&] (Service const &service) {

			Id const service_id { Id::Value("service.", count++) };

			if (service.type != Service::Type::FILE_SYSTEM)
				return;

			bool const fs_visible = (route.browsed.service_id == service_id.value)
			                     || !route.browsed.service_id.valid();
			if (!fs_visible)
				return;

			/*
			 * File system
			 */
			bool const selected = route.selected_service_id == service_id.value
			                   && route.selected_path == "";
			bool const expanded = route.browsed.path.length() > 1;
			bool const has_subdirs = expanded ||
				dir_query.dir_entry_has_sub_dirs(browsed_path_query(component, route),
				                                 service.fs_name());

			Fs_entry entry { service_id, };
			s.widget(entry, service.info, Dir_entry::Attr {
				.level       = 0,
				.selected    = selected,
				.has_subdirs = has_subdirs,
				.expanded    = expanded
			});
		});

		if (route.browsed.path.length() < 2)
			return;

		/*
		 * Path elements towards browsed path
		 */
		unsigned level = 0;
		_for_each_browsed_leading_sub_path(route.browsed.path,
			[&] (Path const &leading, Path const &curr_elem) {
				level++;
				Fs_entry entry { _path_elem_id(route, level) };
				s.widget(entry, curr_elem, Dir_entry::Attr {
					.level       = level,
					.selected    = (without_first_path_element(leading)
					                == route.selected_path),
					.has_subdirs = true,
					.expanded    = true });
			});

		/*
		 * Sub directories of browsed path
		 */
		level++;
		bool dirents_known = false;
		dir_query.for_each_dir_entry(browsed_path_query(component, route),
			[&] (Dir_query::Entry const &dirent) {
				dirents_known = true;
				Path const dirent_path   { route.browsed.path, "/", dirent.name };
				Path const selected_path = without_first_path_element(dirent_path);
				Fs_entry entry { Id { { "l", level, ".", dirent.index } } };
				s.widget(entry, dirent.name, Dir_entry::Attr {
					.level       = level,
					.selected    = (route.selected_path == selected_path),
					.has_subdirs = dirent.num_dirs > 0,
					.expanded    = false }); });

		/*
		 * Keep widget ID during the time between query and response to assist
		 * the animation of directory entries when leaving/entering directories.
		 */
		if (!dirents_known) {
			Fs_entry entry { _path_elem_id(route, level) };
			s.widget(entry, "?", Dir_entry::Attr {
				.level       = level,
				.selected    = false,
				.has_subdirs = false,
				.expanded    = false });
		}
	}

	void click(Clicked_at const &at, Runtime_config const &runtime_config,
	           Dir_query const &dir_query, Component &component, Route &route)
	{
		Id const id = at.matching_id<Vbox, Dir_entry>();

		/* click on top-level file system */
		unsigned count = 0;
		runtime_config.for_each_service([&] (Service const &service) {
			Id const service_id { Id::Value("service.", count++) };
			if (id == service_id) {
				Fs_entry entry { service_id, };
				entry.propagate(at,
					[&] {
						if (route.selected_service_id == id.value
						 && route.selected_path == "") {
							route.deselect();
							return;
						}
						route.selected_service.construct(service);
						route.selected_service_id = id.value;
						route.selected_path = { };
					},
					[&] {
						if (route.browsed.path.length() > 1) {
							route.browsed = { };
						} else {
							route.browsed.path = { "/", service.fs_name() };
							route.browsed.service_id = service_id;
						}
					});
			}
		});

		auto with_browsed_fs_service = [&] (auto const &fn)
		{
			unsigned count = 0;
			runtime_config.for_each_service([&] (Service const &service) {
				Id const service_id { Id::Value("service.", count++) };
				if (route.browsed.service_id == service_id.value)
					fn(service);
			});
		};

		auto toggle_dir_selection = [&] (Path const &selected_path)
		{
			if (route.selected_path == selected_path) {
				route.deselect();
				return;
			}
			with_browsed_fs_service([&] (Service const &service) {
				route.selected_service.construct(service);
				route.selected_service_id = route.browsed.service_id;
				route.selected_path = selected_path;
			});
		};

		/*
		 * Click on path element towards browsed path
		 */
		unsigned level = 0;
		_for_each_browsed_leading_sub_path(route.browsed.path,
			[&] (Path const &leading, Path const &curr) {
				level++;
				Id const path_elem_id = _path_elem_id(route, level);
				if (id != path_elem_id)
					return;

				Fs_entry entry { path_elem_id, };
				entry.propagate(at,
					[&] { toggle_dir_selection(without_first_path_element(leading)); },
					[&] {
						/* close browsed directory (drop last /elem) */
						Path const leading_without_curr {
							Cstring(leading.string(),
							        leading.length() - min(leading.length(), curr.length() + 1)) };
						route.browsed.path = leading_without_curr;
					});
			});

		/*
		 * Click on dir entry of browsed path
		 */
		level++;
		dir_query.for_each_dir_entry(browsed_path_query(component, route),
			[&] (Dir_query::Entry const &dirent) {
				Id const dirent_id { { "l", level, ".", dirent.index } };
				if (id == dirent_id) {
					Path const dirent_path { route.browsed.path, "/", dirent.name };
					Fs_entry entry { dirent_id, };
					entry.propagate(at,
						[&] { toggle_dir_selection(without_first_path_element(dirent_path)); },
						[&] {
							route.browsed.index_at_level(level, dirent.index);
							route.browsed.path = dirent_path;
						});
				}
		});
	}
};

#endif /* _VIEW__FS_ROUTE_WIDGET_H_ */
