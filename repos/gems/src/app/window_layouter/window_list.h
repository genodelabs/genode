/*
 * \brief  Set of present windows
 * \author Norman Feske
 * \date   2018-09-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WINDOW_LIST_H_
#define _WINDOW_LIST_H_

/* local includes */
#include <window.h>
#include <layout_rules.h>

namespace Window_layouter { class Window_list; }


class Window_layouter::Window_list
{
	public:

		struct Change_handler : Interface
		{
			virtual void window_list_changed() = 0;
		};

	private:

		Env                     &_env;
		Allocator               &_alloc;
		Change_handler          &_change_handler;
		Area              const &_screen_size;
		Focus_history           &_focus_history;
		Decorator_margins const &_decorator_margins;

		List_model<Window> _list { };

		Attached_rom_dataspace _rom { _env, "window_list" };

		Signal_handler<Window_list> _rom_handler {
			_env.ep(), *this, &Window_list::_handle_rom };

		void _handle_rom()
		{
			_rom.update();

			/* import window-list changes */
			Update_policy policy(*this);
			_list.update_from_xml(policy, _rom.xml());

			/* notify main program */
			_change_handler.window_list_changed();
		}

		struct Update_policy : List_model<Window>::Update_policy
		{
			Window_list &_window_list;

			Update_policy(Window_list &window_list)
			: _window_list(window_list) { }

			void destroy_element(Window &elem)
			{
				destroy(_window_list._alloc, &elem);
			}

			Window &create_element(Xml_node node)
			{
				unsigned long const id           = node.attribute_value("id", 0UL);
				Area          const initial_size = area_attribute(node);

				Window::Label const label =
					node.attribute_value("label",Window::Label());

				return *new (_window_list._alloc)
					Window(id, label, initial_size,
					       _window_list._focus_history,
					       _window_list._decorator_margins);
			}

			void update_element(Window &win, Xml_node node)
			{
				win.client_size(area_attribute(node));
				win.title      (node.attribute_value("title", Window::Title("")));
				win.has_alpha  (node.attribute_value("has_alpha",  false));
				win.hidden     (node.attribute_value("hidden",     false));
				win.resizeable (node.attribute_value("resizeable", false));
			}

			static bool element_matches_xml_node(Window const &elem, Xml_node node)
			{
				return elem.has_id(node.attribute_value("id", 0UL));
			}
		};

	public:

		Window_list(Env                     &env,
		            Allocator               &alloc,
		            Change_handler          &change_handler,
		            Area              const &screen_size,
		            Focus_history           &focus_history,
		            Decorator_margins const &decorator_margins)
		:
			_env(env),
			_alloc(alloc),
			_change_handler(change_handler),
			_screen_size(screen_size),
			_focus_history(focus_history),
			_decorator_margins(decorator_margins)
		{
			_rom.sigh(_rom_handler);
		}

		void initial_import() { _handle_rom(); }

		void dissolve_windows_from_assignments()
		{
			_list.for_each([&] (Window &win) {
				win.dissolve_from_assignment(); });
		}

		template <typename FN>
		void with_window(Window_id id, FN const &fn)
		{
			_list.for_each([&] (Window &win) {
				if (win.has_id(id))
					fn(win); });
		}

		template <typename FN>
		void with_window(Window_id id, FN const &fn) const
		{
			_list.for_each([&] (Window const &win) {
				if (win.has_id(id))
					fn(win); });
		}

		template <typename FN>
		void for_each_window(FN const &fn) { _list.for_each(fn); }

		template <typename FN>
		void for_each_window(FN const &fn) const { _list.for_each(fn); }
};

#endif /* _WINDOW_LIST_H_ */
