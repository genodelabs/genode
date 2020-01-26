/*
 * \brief  File-browser dialog
 * \author Norman Feske
 * \date   2020-01-31
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
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


struct Sculpt::File_browser_dialog : Noncopyable, Dialog
{
	Runtime_config     const &_runtime_config;
	File_browser_state const &_state;

	Hoverable_item _fs_button     { };
	Hoverable_item _entry         { };
	Hoverable_item _path_elem     { };
	Hoverable_item _action_button { };

	void reset() override { }

	using Fs_name = File_browser_state::Fs_name;
	using Index   = File_browser_state::Index;
	using Sub_dir = File_browser_state::Path;
	using Path    = File_browser_state::Path;
	using File    = File_browser_state::Path;

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

	using Name = String<128>;

	template <typename FN>
	void _for_each_path_elem(FN const &fn) const
	{
		char const *curr = _state.path.string();

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

	Path _path_elem_at_index(unsigned index) const
	{
		Path result { };
		unsigned i = 0;

		_for_each_path_elem([&] (Path const &elem) {
			if (i++ == index)
				result = elem; });

		return result;
	}

	void _gen_path_elements(Xml_generator &xml) const
	{
		gen_named_node(xml, "hbox", "back", [&] () {
			unsigned i = 0;
			_for_each_path_elem([&] (Path const elem) {

				gen_named_node(xml, "button", Index(i), [&] () {
					if (!_state.modified)
						_path_elem.gen_hovered_attr(xml, Index(i));
					xml.node("label", [&] () {
						xml.attribute("text", elem);
					});
				});
				i++;
			});
		});
	}

	void _gen_back_entry(Xml_generator &xml) const
	{
		gen_named_node(xml, "hbox", "back", [&] () {

			gen_named_node(xml, "float", "left", [&] () {
				xml.attribute("west", "yes");

				xml.node("hbox", [&] () {
					if (!_state.modified) {
						gen_named_node(xml, "button", "button", [&] () {
							xml.attribute("style", "back");
							_entry.gen_hovered_attr(xml, "back");
							xml.node("hbox", [&] () { });
						});
					}

					_gen_path_elements(xml);
				});
			});

			gen_named_node(xml, "hbox", "right", [&] () { });
		});
	}

	void _gen_entry(Xml_generator &xml, Xml_node node, Index const &index,
	                char const *style) const
	{
		Name const name = node.attribute_value("name", Name());

		bool const entry_edited = (name == _state.edited_file);

		/* while editing one file, hide all others */
		if (!entry_edited && _state.modified)
			return;

		gen_named_node(xml, "hbox", index, [&] () {

			gen_named_node(xml, "float", "left", [&] () {
				xml.attribute("west", "yes");

				xml.node("hbox", [&] () {
					gen_named_node(xml, "button", "button", [&] () {

						bool const selected = (name == _state.edited_file);
						if (selected)
							xml.attribute("selected", "yes");

						xml.attribute("style", style);

						if (!_state.modified)
							_entry.gen_hovered_attr(xml, index);

						xml.node("hbox", [&] () { });
					});
					gen_named_node(xml, "label", "name", [&] () {
						xml.attribute("text", Path(" ", name)); });
				});
			});

			gen_named_node(xml, "hbox", "middle", [&] () { });

			gen_named_node(xml, "float", "right", [&] () {
				xml.attribute("east", "yes");

				/* show no operation buttons for directories */
				if (node.type() == "dir")
					return;

				gen_named_node(xml, "hbox", "right", [&] () {

					bool const entry_hovered = _entry.hovered(index);
					bool const interesting   = entry_hovered || entry_edited;
					bool const writeable     = node.attribute_value("writeable", false);

					if (writeable) {

						if (!_state.modified) {
							gen_named_node(xml, "button", "edit", [&] () {
								if (interesting) {
									_action_button.gen_hovered_attr(xml, "edit");
									if (entry_edited)
										xml.attribute("selected", "yes");
								} else {
									xml.attribute("style", "invisible");
								}

								gen_named_node(xml, "label", "name", [&] () {
									xml.attribute("text", "Edit");
									if (!interesting)
										xml.attribute("style", "invisible");
								});
							});
						}

						if (entry_edited && _state.modified) {

							auto gen_action_button = [&] (auto id, auto text)
							{
								gen_named_node(xml, "button", id, [&] () {
									_action_button.gen_hovered_attr(xml, id);
									gen_named_node(xml, "label", "name", [&] () {
										xml.attribute("text", text); }); });
							};

							gen_action_button("revert", "Revert");
							gen_action_button("save",   "Save");
						}

					} else {
						gen_named_node(xml, "button", "view", [&] () {

							if (interesting) {
								 _action_button.gen_hovered_attr(xml, "view");
								if (entry_edited)
									xml.attribute("selected", "yes");
							} else {
								xml.attribute("style", "invisible");
							}

							gen_named_node(xml, "label", "name", [&] () {
								xml.attribute("text", "View");
								if (!interesting)
									xml.attribute("style", "invisible");
							});
						});
					}
				});
			});
		});
	}

	void _gen_file_system(Xml_generator &xml, Service const &service) const
	{
		bool  const parent = !service.server.valid();
		Label const name   = parent ? Start_name(service.label) : service.server;

		Start_name const pretty_name { Pretty(name) };

		bool const selected = (_state.browsed_fs == name);

		if (_state.text_area.constructed() && _state.modified && !selected)
			return;

		gen_named_node(xml, "frame", name, [&] () {
			xml.node("vbox", [&] () {
				xml.node("label", [&] () {
					xml.attribute("min_ex", "50"); });

				gen_named_node(xml, "button", name, [&] () {
					if (!_state.modified)
						_fs_button.gen_hovered_attr(xml, name);

					if (selected)
						xml.attribute("selected", true);

					xml.node("label", [&] () {
						xml.attribute("text", pretty_name);
						xml.attribute("style", "title");
					});
				});

				if (selected) {
					unsigned cnt = 0;

					_state.with_query_result([&] (Xml_node node) {
						node.with_sub_node("dir", [&] (Xml_node listing) {

							if (_state.path != "/")
								_gen_back_entry(xml);

							listing.for_each_sub_node("dir", [&] (Xml_node dir) {
								_gen_entry(xml, dir, Index(cnt++), "enter"); });

							listing.for_each_sub_node("file", [&] (Xml_node file) {
								_gen_entry(xml, file, Index(cnt++), "radio"); });
						});
					});
				}
			});
		});
	}

	void generate(Xml_generator &xml) const override
	{
		xml.node("vbox", [&] () {
			_runtime_config.for_each_service([&] (Service const &service) {
				if (service.type == Service::Type::FILE_SYSTEM)
					_gen_file_system(xml, service); }); });
	}

	Hover_result hover(Xml_node hover) override
	{
		return any_hover_changed(
			_entry.match(hover, "vbox", "frame", "vbox", "hbox", "name"),
			_path_elem.match(hover, "vbox", "frame", "vbox", "hbox", "float", "hbox", "hbox", "button", "name"),
			_fs_button.match(hover, "vbox", "frame", "vbox", "button", "name"),
			_action_button.match(hover, "vbox", "frame", "vbox", "hbox", "float", "hbox", "button", "name"));
	}

	Click_result click(Action &action)
	{
		Click_result result = Click_result::IGNORED;

		Fs_name const fs_name = _fs_button._hovered;
		if (fs_name.length() > 1) {
			if (!_state.modified)
				action.browse_file_system(fs_name);
			return Click_result::CONSUMED;
		}

		Index const path_elem_idx = _path_elem._hovered;
		if (path_elem_idx.length() > 1) {

			Genode::Path<256> abs_path { "/" };
			for (unsigned i = 0; i < 20; i++) {
				abs_path.append_element(_path_elem_at_index(i).string());
				if (Index(i) == path_elem_idx)
					break;
			}
			if (!_state.modified)
				action.browse_abs_directory(Path(abs_path));

			return Click_result::CONSUMED;
		}

		Index const index = _entry._hovered;
		if (index == "back") {
			if (!_state.modified)
				action.browse_parent_directory();
		}
		else if (index.length() > 1) {
			_state.with_entry_at_index(index, [&] (Xml_node entry) {

				if (entry.has_type("dir"))
					action.browse_sub_directory(entry.attribute_value("name", Sub_dir()));

				if (entry.has_type("file")) {
					if (_action_button.hovered("edit"))
						action.edit_file(entry.attribute_value("name", Name()));

					if (_action_button.hovered("view"))
						action.view_file(entry.attribute_value("name", Name()));

					if (_action_button.hovered("revert"))
						action.revert_edited_file();

					if (_action_button.hovered("save"))
						action.save_edited_file();
				}
			});

			return Click_result::CONSUMED;
		}

		return result;
	}

	File_browser_dialog(Runtime_config     const &runtime_config,
	                    File_browser_state const &state)
	:
		_runtime_config(runtime_config), _state(state)
	{ }
};

#endif /* _VIEW__FILE_BROWSER_H_ */
