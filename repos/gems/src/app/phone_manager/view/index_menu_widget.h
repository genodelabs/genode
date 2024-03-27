/*
 * \brief  Widget for browsing a depot index
 * \author Norman Feske
 * \date   2023-03-21
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__INDEX_MENU_WIDGET_H_
#define _VIEW__INDEX_MENU_WIDGET_H_

#include <view/dialog.h>
#include <model/index_menu.h>

namespace Sculpt { struct Index_menu_widget; }


struct Sculpt::Index_menu_widget : Widget<Vbox>
{
	public:

		using User  = Depot::Archive::User;
		using Index = Rom_data;
		using Name  = Start_name;

		struct Sub_menu_title : Widget<Left_floating_hbox>
		{
			void view(Scope<Left_floating_hbox> &s, auto const &text) const
			{
				bool const hovered = (s.hovered() && !s.dragged());

				s.sub_scope<Icon>("back", Icon::Attr { .hovered  = hovered,
				                                       .selected = true });
				s.sub_scope<Label>(" ");
				s.sub_scope<Label>(text, [&] (auto &s) {
					s.attribute("font", "title/regular"); });

				/* inflate vertical space to button size */
				s.sub_scope<Button>([&] (Scope<Left_floating_hbox, Button> &s) {
					s.attribute("style", "invisible");
					s.sub_scope<Label>(""); });
			}

			void click(Clicked_at const &, auto const &fn) const { fn(); }
		};

	private:

		Index const &_index;

		Index_menu _menu { };

		bool _pkg_selected = false;

		Hosted<Vbox, Sub_menu_title> const _back { Id { "back" } };

		void _reset_selection() { _pkg_selected = false; }

		void _for_each_menu_item(User const &user, auto const &fn) const
		{
			_index.with_xml([&] (Xml_node const &index) {
				_menu.for_each_item(index, user, fn); });
		}

	public:

		Index_menu_widget(Index const &index) : _index(index) { }

		void view(Scope<Vbox> &s, User const &user, auto const &view_item_fn) const
		{
			if (_menu.level())
				s.widget(_back, Name { _menu });

			unsigned count = 0;
			_for_each_menu_item(user, [&] (Xml_node const &item) {

				Id const id { { count } };

				if (item.has_type("index")) {
					auto const name = item.attribute_value("name", Name());

					view_item_fn(s, id, Name { name, " ..." }, "");
				}

				if (item.has_type("pkg")) {
					auto const path = item.attribute_value("path", Depot::Archive::Path());
					auto const name = Depot::Archive::name(path);

					view_item_fn(s, id, name, path);
				}
				count++;
			});
		}

		void click(Clicked_at const &at, User const &user,
		           auto const &enter_pkg_fn,
		           auto const &leave_pkg_fn,
		           auto const &pkg_operation_fn)
		{
			/* go one menu up */
			_back.propagate(at, [&] {
				_menu._selected[_menu._level] = Index_menu::Name();
				_menu._level--;
				_pkg_selected = false;
				leave_pkg_fn();
			});

			/* enter sub menu of index */
			if (_menu._level < Index_menu::MAX_LEVELS - 1) {

				Id const clicked = at.matching_id<Vbox, Menu_entry>();

				unsigned count = 0;
				_for_each_menu_item(user, [&] (Xml_node const &item) {

					if (clicked == Id { { count } }) {

						if (item.has_type("index")) {

							Index_menu::Name const name =
								item.attribute_value("name", Index_menu::Name());

							_menu._selected[_menu._level] = name;
							_menu._level++;

						} else if (item.has_type("pkg")) {

							_pkg_selected = true;
							enter_pkg_fn(item);
						}
					}
					count++;
				});

				if (at.matching_id<Vbox, Float>() == Id { "pkg" })
					pkg_operation_fn(at);
			}
		}

		void clack(Clacked_at const &at, auto const &pkg_operation_fn)
		{
			if (at.matching_id<Vbox, Float>() == Id { "pkg" })
				pkg_operation_fn(at);
		}

		bool top_level() const { return (_menu.level() == 0) && !_pkg_selected; }

		bool pkg_selected() const { return _pkg_selected; }

		void deselect_pkg() { _pkg_selected = false; }

		void reset()
		{
			_menu = Index_menu { };
			_reset_selection();
		}

		void one_level_back()
		{
			if (_menu.level() == 0)
				_menu._level--;

			_reset_selection();
		}

		bool anything_visible(User const &user) const
		{
			if (_menu.level())
				return true;

			bool at_least_one_item_exists = false;
			_for_each_menu_item(user, [&] (Xml_node const &) {
				at_least_one_item_exists = true; });

			return at_least_one_item_exists;
		}
};

#endif /* _VIEW__INDEX_MENU_WIDGET_H_ */
