/*
 * \brief  Dialog for software update
 * \author Norman Feske
 * \date   2023-01-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_UPDATE_DIALOG_H_
#define _VIEW__SOFTWARE_UPDATE_DIALOG_H_

#include <model/nic_state.h>
#include <model/build_info.h>
#include <model/download_queue.h>
#include <model/index_update_queue.h>
#include <view/depot_users_dialog.h>

namespace Sculpt { struct Software_update_dialog; }


struct Sculpt::Software_update_dialog : Widget<Vbox>
{
	using Depot_users     = Depot_users_dialog::Depot_users;
	using User            = Depot_users_dialog::User;
	using Image_index     = Attached_rom_dataspace;
	using Url             = Depot_users_dialog::Url;
	using Version         = String<16>;
	using User_properties = Depot_users_dialog::User_properties;

	Build_info const _build_info;

	Nic_state            const &_nic_state;
	Download_queue       const &_download_queue;
	Index_update_queue   const &_index_update_queue;
	File_operation_queue const &_file_operation_queue;
	Image_index          const &_image_index;

	struct Action : Interface
	{
		virtual void query_image_index     (User const &) = 0;
		virtual void trigger_image_download(Path const &, Verify) = 0;
		virtual void update_image_index    (User const &, Verify) = 0;
		virtual void install_boot_image    (Path const &) = 0;
	};

	Action &_action;

	Depot_users_dialog _users;

	Path _last_installed { };
	Path _last_selected  { };

	Path _index_path() const { return Path(_users.selected(), "/image/index"); }

	bool _index_update_in_progress() const
	{
		using Update = Index_update_queue::Update;

		bool result = false;
		_index_update_queue.with_update(_index_path(), [&] (Update const &update) {
			if (update.active())
				result = true; });

		return result;
	}

	Path _image_path(Version const &version) const
	{
		return Path(_users.selected(), "/image/sculpt-", _build_info.board, "-", version);
	}

	bool _installing() const
	{
		return _file_operation_queue.copying_to_path("/rw/boot");
	};

	Hoverable_item _check     { };
	Hoverable_item _version   { };
	Hoverable_item _operation { };

	using Hover_result = Hoverable_item::Hover_result;

	Software_update_dialog(Build_info           const &build_info,
	                       Nic_state            const &nic_state,
	                       Download_queue       const &download_queue,
	                       Index_update_queue   const &index_update_queue,
	                       File_operation_queue const &file_operation_queue,
	                       Depot_users          const &depot_users,
	                       Image_index          const &image_index,
	                       Depot_users_dialog::Action &depot_users_action,
	                       Action &action)
	:
		_build_info(build_info), _nic_state(nic_state),
		_download_queue(download_queue),
		_index_update_queue(index_update_queue),
		_file_operation_queue(file_operation_queue),
		_image_index(image_index),
		_action(action),
		_users(depot_users, _build_info.depot_user, depot_users_action)
	{ }

	static void _gen_vspacer(Xml_generator &xml, char const *name)
	{
		gen_named_node(xml, "label", name, [&] () {
			xml.attribute("text", " ");
			xml.attribute("font", "annotation/regular");
		});
	}

	void _gen_image_main(Xml_generator &xml, Xml_node const &image) const
	{
		Version const version = image.attribute_value("version", Version());
		bool    const present = image.attribute_value("present", false);
		Path    const path    = _image_path(version);

		struct Download_state
		{
			bool     in_progress;
			bool     failed;
			unsigned percent;
		};

		using Download = Download_queue::Download;

		auto state_from_download_queue = [&]
		{
			Download_state result { };
			_download_queue.with_download(path, [&] (Download const &download) {

				if (download.state == Download::State::DOWNLOADING)
					result.in_progress = true;

				if (download.state == Download::State::FAILED)
					result.failed = true;

				result.percent = download.percent;
			});
			return result;
		};

		Download_state const download_state = state_from_download_queue();

		gen_named_node(xml, "float", "label", [&] {
			xml.attribute("west", "yes");
			gen_named_node(xml, "label", "label", [&] {
				xml.attribute("text", String<50>("  ", version));
				xml.attribute("min_ex", "15");
			});
		});

		auto gen_status = [&] (auto message)
		{
			gen_named_node(xml, "float", "status", [&] {
				xml.node("label", [&] {
					xml.attribute("font", "annotation/regular");
					xml.attribute("text", message); }); });
		};

		if (image.has_sub_node("info")) {
			if (_last_selected == path)
				gen_status("Changes");
			else
				gen_status("...");
		}

		if (download_state.in_progress && download_state.percent)
			gen_status(String<16>(download_state.percent, "%"));

		if (download_state.failed)
			gen_status("unavailable");

		if (_last_installed == path) {
			if (_installing())
				gen_status("installing...");
			else
				gen_status("reboot to activate");
		}

		gen_named_node(xml, "float", "buttons", [&] {
			xml.attribute("east", "yes");
			xml.node("hbox", [&] {

				auto gen_button = [&] (auto id, bool selected, auto text)
				{
					gen_named_node(xml, "button", id, [&] {

						if (version == _version._hovered)
							_operation.gen_hovered_attr(xml, id);

						if (selected) {
							xml.attribute("selected", "yes");
							xml.attribute("style", "unimportant");
						}

						xml.node("label", [&] {
							xml.attribute("text", text); });
					});
				};

				if (present)
					gen_button("install", _installing(), "  Install  ");

				if (!present)
					gen_button("download", download_state.in_progress, "  Download  ");
			});
		});
	}

	void _gen_image_info(Xml_generator &xml, Xml_node const &image) const
	{
		gen_named_node(xml, "vbox", "main", [&] {

			unsigned line = 0;

			image.for_each_sub_node("info", [&] (Xml_node const &info) {

				/* limit changelog to a sensible maximum of lines  */
				if (++line > 8)
					return;

				using Text = String<80>;
				Text const text = info.attribute_value("text", Text());

				gen_named_node(xml, "float", String<16>(line), [&] {
					xml.attribute("west", "yes");
					xml.node("label", [&] {
						xml.attribute("text", text);
						xml.attribute("font", "annotation/regular");
					});
				});
			});
		});
	}

	void _gen_image_entry(Xml_generator &xml, Xml_node const &image) const
	{
		Version const version = image.attribute_value("version", Version());
		Path    const path    = _image_path(version);

		gen_named_node(xml, "frame", version, [&] {
			xml.attribute("style", "important");

			xml.node("vbox", [&] {

				gen_named_node(xml, "float", "main", [&] {
					xml.attribute("east", "yes");
					xml.attribute("west", "yes");
					_gen_image_main(xml, image);
				});

				if (path == _last_selected && image.has_sub_node("info")) {
					_gen_vspacer(xml, "above");
					gen_named_node(xml, "float", "info", [&] {
						_gen_image_info(xml, image); });
					_gen_vspacer(xml, "below");
				}
			});
		});
	}

	void _gen_image_list(Xml_generator &xml) const
	{
		Xml_node const index = _image_index.xml();

		index.for_each_sub_node("user", [&] (Xml_node const &user) {
			if (user.attribute_value("name", User()) == _users.selected())
				user.for_each_sub_node("image", [&] (Xml_node const &image) {
					_gen_image_entry(xml, image); }); });
	}

	void _gen_update_dialog(Xml_generator &xml) const
	{
		gen_named_node(xml, "frame", "update_dialog", [&] {
			xml.node("vbox", [&] {
				_users.generate(xml);

				User_properties const properties = _users.selected_user_properties();

				bool const offer_index_update = _users.one_selected()
				                             && _nic_state.ready()
				                             && properties.download_url;
				if (offer_index_update) {
					_gen_vspacer(xml, "above check");
					gen_named_node(xml, "float", "check", [&] {
						gen_named_node(xml, "button", "check", [&] {
							_check.gen_hovered_attr(xml, "check");
							if (_index_update_in_progress()) {
								xml.attribute("selected", "yes");
								xml.attribute("style", "unimportant");
							}
							xml.node("label", [&] {
								auto const text = properties.public_key
								                ? "  Check for Updates  "
								                : "  Check for unverified Updates  ";
								xml.attribute("text", text); });
						});
					});
					_gen_vspacer(xml, "below check");
				}
			});
		});

		_gen_image_list(xml);
	}

	void generate(Xml_generator &xml) const
	{
		gen_named_node(xml, "vbox", "update", [&] {
			_gen_update_dialog(xml); });
	}

	Hover_result hover(Xml_node const &hover)
	{
		_users.reset_hover();

		return Deprecated_dialog::any_hover_changed(
			match_sub_dialog(hover, _users, "vbox", "frame", "vbox"),
			_check    .match(hover, "vbox", "frame", "vbox", "float", "button", "name"),
			_version  .match(hover, "vbox", "frame", "name"),
			_operation.match(hover, "vbox", "frame", "vbox", "float", "float", "hbox", "button", "name")
		);
	}

	bool hovered() const { return _users.hovered();  }

	void click()
	{
		Verify const verify { _users.selected_user_properties().public_key };

		if (_users.hovered())
			_users.click([&] (User const &selected_user) {
				_action.query_image_index(selected_user); });

		if (_check.hovered("check") && !_index_update_in_progress())
			_action.update_image_index(_users.selected(), verify);

		if (_operation.hovered("download"))
			_action.trigger_image_download(_image_path(_version._hovered), verify);

		if (_version._hovered.length() > 1)
			_last_selected = _image_path(_version._hovered);

		if (_operation.hovered("install") && !_installing()) {
			_last_installed = _image_path(_version._hovered);
			_action.install_boot_image(_last_installed);
		}
	}

	void clack() { }

	bool keyboard_needed() const { return _users.keyboard_needed(); }

	void handle_key(Codepoint c) { _users.handle_key(c); }

	void sanitize_user_selection() { _users.sanitize_unfold_state(); }

	void reset()
	{
		_last_installed = { };
		_last_selected  = { };
	}
};

#endif /* _VIEW__SOFTWARE_UPDATE_DIALOG_H_ */
