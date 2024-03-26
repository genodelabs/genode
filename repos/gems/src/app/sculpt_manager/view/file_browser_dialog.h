/*
 * \brief  File-browser dialog
 * \author Norman Feske
 * \date   2020-01-31
 */

/*
 * Copyright (C) 2020-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__FILE_BROWSER_DIALOG_H_
#define _VIEW__FILE_BROWSER_DIALOG_H_

#include <view/dialog.h>
#include <model/runtime_config.h>
#include <model/file_browser_state.h>

namespace Sculpt { struct File_browser_dialog; }


struct Sculpt::File_browser_dialog : Top_level_dialog
{
	Runtime_config     const &_runtime_config;
	File_browser_state const &_state;

	using Fs_name = File_browser_state::Fs_name;
	using Sub_dir = File_browser_state::Path;
	using Path    = File_browser_state::Path;
	using File    = File_browser_state::Path;
	using Name    = String<128>;

	struct Action : Interface, Noncopyable
	{
		virtual void browse_file_system(Fs_name const &) = 0;
		virtual void browse_sub_directory(Sub_dir const &) = 0;
		virtual void browse_abs_directory(Path const &) = 0;
		virtual void browse_parent_directory() = 0;
		virtual void view_file(File const &) = 0;
		virtual void edit_file(File const &) = 0;
		virtual void revert_edited_file() = 0;
		virtual void save_edited_file() = 0;
	};

	Action &_action;

	static void _for_each_path_elem(Path const path, auto const &fn)
	{
		char const *curr = path.string();

		fn(Path("/"));

		for (;;) {

			if (*curr != '/')
				return;

			curr++; /* skip '/' */

			/* search next '/' */
			size_t len = 0;
			for (; curr[len] != 0 && curr[len] != '/'; len++);

			fn(Path(Cstring(curr, len)));

			curr += len;
		}
	}

	static Path _path_elem_at_index(Path const path, unsigned index)
	{
		Path result { };
		unsigned i = 0;

		_for_each_path_elem(path, [&] (Path const &elem) {
			if (i++ == index)
				result = elem; });

		return result;
	}

	static void _with_matching_entry(At const &at, auto const fn)
	{
		Id const entry_id = at.matching_id<Vbox, Frame, Vbox, Entry>();
		unsigned index = ~0U;
		if (ascii_to(entry_id.value.string(), index)) {
			Hosted_entry entry { entry_id, index };
			fn(entry);
		}
	}

	struct Navigation_entry : Widget<Left_floating_hbox>
	{
		struct Back : Widget<Float>
		{
			void view(Scope<Float> &s) const
			{
				s.sub_scope<Button>([&] (Scope<Float, Button> &s) {
					if (s.hovered()) s.attribute("hovered", "yes");
					s.attribute("style", "back");
					s.sub_scope<Hbox>();
				});
			}

			void click(Clicked_at const &, auto const &fn) { fn(); }
		};

		Hosted<Left_floating_hbox, Back> _back { Id { "back" } };

		void view(Scope<Left_floating_hbox> &s, Path const &path, bool allow_back) const
		{
			if (allow_back)
				s.widget(_back);

			/* don't hover last path element */
			unsigned count = 0;
			_for_each_path_elem(path, [&] (Path const &) { count++; });

			unsigned i = 0;
			_for_each_path_elem(path, [&] (Path const elem) {
				Id const id { { i++ } };
				bool const last = (i == count);
				s.sub_scope<Button>(id, [&] (Scope<Left_floating_hbox, Button> &s) {

					if (allow_back && s.hovered() && !last)
						s.attribute("hovered", "yes");

					if (last)
						s.attribute("style", "unimportant");

					s.sub_scope<Label>(elem);
				});
			});
		}

		void click(Clicked_at const &at, Path const &path, Action &action)
		{
			Id const elem_id = at.matching_id<Left_floating_hbox, Button>();
			if (elem_id.valid()) {
				Genode::Path<256> abs_path { "/" };
				for (unsigned i = 0; i < 20; i++) {
					abs_path.append_element(_path_elem_at_index(path, i).string());
					if (Id { { i } } == elem_id)
						break;
				}
				action.browse_abs_directory(Path(abs_path));
			}

			_back.propagate(at, [&] { action.browse_parent_directory(); });
		}
	};

	struct Conditional_button : Widget<Button>
	{
		void view(Scope<Button> &s, bool condition, bool selected) const
		{
			if (s.hovered()) s.attribute("hovered",  "yes");
			if (selected)    s.attribute("selected", "yes");

			if (!condition)
				s.attribute("style", "invisible");

			s.sub_scope<Label>(s.id.value, [&] (auto &s) {
				if (!condition) s.attribute("style", "invisible"); });
		}

		void click(Clicked_at const &, auto const &fn) { fn(); }
	};

	/*
	 * Use button instances accross entries to keep their internal state
	 * independent from the lifetime of 'Entry' objects, which exist only
	 * temporarily.
	 */
	struct Entry_buttons
	{
		/* scope hierarchy relative to 'Entry' */
		Hosted<Hbox, Float, Hbox, Conditional_button>
			edit   { Id { "Edit"   } },
			view   { Id { "View"   } };

		Hosted<Hbox, Float, Hbox, Deferred_action_button>
			revert { Id { "Revert" } },
			save   { Id { "Save"   } };

	} _entry_buttons { };

	struct Entry : Widget<Hbox>
	{
		unsigned const index;

		Entry(unsigned index) : index(index) { }

		void view(Scope<Hbox> &s, File_browser_state const &state,
		          Xml_node const &node, auto const &style,
		          Entry_buttons const &buttons) const
		{
			Name const name     = node.attribute_value("name", Name());
			bool const hovered  = (s.hovered() && !state.modified);
			bool const selected = (name == state.edited_file);

			/* while editing one file, hide all others */
			if (!selected && state.modified)
				return;

			s.sub_scope<Float>([&] (Scope<Hbox, Float> &s) {
				s.attribute("west", "yes");
				s.sub_scope<Hbox>([&] (Scope<Hbox, Float, Hbox> &s) {
					s.sub_scope<Icon>(style, Icon::Attr { .hovered  = hovered,
					                                      .selected = selected });
					s.sub_scope<Label>(Path(" ", name)); }); });

			s.sub_scope<Float>([&] (Scope<Hbox, Float> &s) {
				s.attribute("east", "yes");

				/* show no operation buttons for directories */
				if (node.type() == "dir")
					return;

				s.sub_scope<Hbox>([&] (Scope<Hbox, Float, Hbox> &s) {

					bool const interesting = hovered || selected;
					bool const writeable   = node.attribute_value("writeable", false);

					if (writeable) {
						if (!state.modified)
							s.widget(buttons.edit, interesting, selected);

						if (selected && state.modified) {
							s.widget(buttons.revert);
							s.widget(buttons.save);
						}
					} else {
						s.widget(buttons.view, interesting, selected);
					}
				});
			});
		}

		void click(Clicked_at const &at, Xml_node const &node,
		           Entry_buttons &buttons, Action &action)
		{
			Name const name = node.attribute_value("name", Name());

			if (node.has_type("dir")) {
				action.browse_sub_directory(name);
				return;
			}

			buttons.edit  .propagate(at, [&] { action.edit_file(name); });
			buttons.view  .propagate(at, [&] { action.view_file(name); });
			buttons.revert.propagate(at);
			buttons.save  .propagate(at);
		}

		void clack(Clacked_at const &at, Entry_buttons &buttons, Action &action)
		{
			buttons.revert.propagate(at, [&] { action.revert_edited_file(); });
			buttons.save  .propagate(at, [&] { action.save_edited_file();   });
		}
	};

	Hosted<Vbox, Frame, Vbox, Navigation_entry> _nav_entry { Id { "nav" } };

	using Hosted_entry = Hosted<Vbox, Frame, Vbox, Entry>;

	void _view_file_system(Scope<Vbox> &s, Service const &service) const
	{
		bool       const parent = !service.server.valid();
		Start_name const name   = parent ? Start_name(service.label) : service.server;
		Start_name const pretty_name { Pretty(name) };
		bool       const selected = (_state.browsed_fs == name);

		if (_state.text_area.constructed() && _state.modified && !selected)
			return;

		s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {
				s.sub_scope<Min_ex>(50);
				s.sub_scope<Button>(Id { name }, [&] (Scope<Vbox, Frame, Vbox, Button> &s) {

					if (!_state.modified && s.hovered())
						s.attribute("hovered", "yes");

					if (selected)
						s.attribute("selected", true);

					s.sub_scope<Label>(pretty_name, [&] (auto &s) {
						s.attribute("style", "title"); });
				});

				if (selected) {
					unsigned count = 0;

					_state.with_query_result([&] (Xml_node const &node) {
						node.with_optional_sub_node("dir", [&] (Xml_node const &listing) {

							if (_state.path != "/")
								s.widget(_nav_entry, _state.path, !_state.modified);

							listing.for_each_sub_node("dir", [&] (Xml_node const &dir) {
								unsigned const index = count++;
								Hosted_entry entry { Id { { index } }, index };
								s.widget(entry, _state, dir, "enter", _entry_buttons); });

							listing.for_each_sub_node("file", [&] (Xml_node const &file) {
								unsigned const index = count++;
								Hosted_entry entry { Id { { index } }, index };
								s.widget(entry, _state, file, "radio", _entry_buttons); });
						});
					});
				}
			});
		});
	}

	void view(Scope<> &s) const override
	{
		s.sub_scope<Vbox>([&] (Scope<Vbox> &s) {
			_runtime_config.for_each_service([&] (Service const &service) {
				if (service.type == Service::Type::FILE_SYSTEM)
					_view_file_system(s, service); }); });
	}

	File_browser_dialog(Runtime_config     const &runtime_config,
	                    File_browser_state const &state,
	                    Action                   &action)
	:
		Top_level_dialog("file_browser"),
		_runtime_config(runtime_config), _state(state), _action(action)
	{ }

	void click(Clicked_at const &at) override
	{
		Id const fs_id = at.matching_id<Vbox, Frame, Vbox, Button>();
		if (fs_id.valid()) {
			if (!_state.modified)
				_action.browse_file_system(fs_id.value);
			return;
		}

		if (!_state.modified)
			_nav_entry.propagate(at, _state.path, _action);

		_with_matching_entry(at, [&] (Hosted_entry &entry) {
			_state.with_entry_at_index(entry.index, [&] (Xml_node const &node) {
				entry.propagate(at, node, _entry_buttons, _action); }); });
	}

	void clack(Clacked_at const &at) override
	{
		_with_matching_entry(at, [&] (Hosted_entry &entry) {
			entry.propagate(at, _entry_buttons, _action); });
	}

	void drag(Dragged_at const &) override { }
};

#endif /* _VIEW__FILE_BROWSER_H_ */
