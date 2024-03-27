/*
 * \brief  Widget for software update
 * \author Norman Feske
 * \date   2023-01-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_UPDATE_WIDGET_H_
#define _VIEW__SOFTWARE_UPDATE_WIDGET_H_

#include <model/nic_state.h>
#include <model/build_info.h>
#include <model/download_queue.h>
#include <model/index_update_queue.h>
#include <view/depot_users_widget.h>

namespace Sculpt { struct Software_update_widget; }


struct Sculpt::Software_update_widget : Widget<Vbox>
{
	using Depot_users = Depot_users_widget::Depot_users;
	using User        = Depot_users_widget::User;
	using Version     = String<16>;

	Build_info const _build_info;

	Nic_state            const &_nic_state;
	Download_queue       const &_download_queue;
	Index_update_queue   const &_index_update_queue;
	File_operation_queue const &_file_operation_queue;

	Path _last_installed { };
	Path _last_selected  { };

	Hosted<Vbox, Frame, Vbox, Depot_users_widget> _users;

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
	}

	struct Download_state
	{
		bool     in_progress;
		bool     failed;
		unsigned percent;

		static Download_state from_queue(Download_queue const &queue, Path const &path)
		{
			using Download = Download_queue::Download;

			Download_state result { };
			queue.with_download(path, [&] (Download const &download) {
				result = {
					.in_progress = (download.state == Download::State::DOWNLOADING),
					.failed      = (download.state == Download::State::FAILED),
					.percent     = download.percent
				};
			});
			return result;
		}
	};

	struct Image_main : Widget<Float>
	{
		struct Attr
		{
			Version        version;
			Path           path;
			bool           present;
			bool           changelog;
			Download_state download_state;
			bool           selected;
			bool           last_installed;
			bool           installing;
		};

		Hosted<Float, Right_floating_hbox, Operation_button> const
			_install  { Id { "Install"  } },
			_download { Id { "Download" } };

		void view(Scope<Float> &s, Attr const &attr) const
		{
			s.attribute("east", "yes");
			s.attribute("west", "yes");

			s.sub_scope<Left_floating_text>(attr.version);

			auto status_message = [] (Attr const &attr) -> String<50>
			{
				if (attr.last_installed)
					return { attr.installing ? "installing..." : "reboot to activate" };

				if (attr.download_state.failed)
					return { "unavailable" };

				if (attr.download_state.in_progress && attr.download_state.percent)
					return { attr.download_state.percent, "%" };

				if (attr.changelog)
					return { attr.selected ? "Changes" : "..." };

				return { };
			};

			s.sub_scope<Float>([&] (Scope<Float, Float> &s) {
				s.sub_scope<Annotation>(status_message(attr)); });

			s.sub_scope<Right_floating_hbox>([&] (Scope<Float, Right_floating_hbox> &s) {

				if (attr.present)
					s.widget(_install, attr.installing);
				else
					s.widget(_download, attr.download_state.in_progress);
			});
		}

		void click(Clicked_at const &at,
		           auto const &install_fn, auto const &download_fn) const
		{
			_install .propagate(at, install_fn);
			_download.propagate(at, download_fn);
		}
	};

	struct Changelog : Widget<Float>
	{
		void view(Scope<Float> &s, Xml_node const &image) const
		{
			using Text = String<80>;

			unsigned line = 0; /* limit changelog to a sensible max. of lines */

			s.sub_scope<Vbox>([&] (Scope<Float, Vbox> &s) {
				s.sub_scope<Small_vgap>();
				image.for_each_sub_node("info", [&] (Xml_node const &info) {
					if (++line < 9)
						s.sub_scope<Left_annotation>(info.attribute_value("text", Text()));
				});
				s.sub_scope<Small_vgap>();
			});
		}
	};

	/* used accross images */
	Hosted<Vbox, Frame, Vbox, Image_main> const _hosted_image_main { Id { "main" } };

	void _view_image_entry(Scope<Vbox> &s, Xml_node const &image) const
	{
		Version const version = image.attribute_value("version", Version());
		Path    const path    = _image_path(version);

		Image_main::Attr const attr {
			.version        = version,
			.path           = path,
			.present        = image.attribute_value("present", false),
			.changelog      = image.has_sub_node("info"),
			.download_state = Download_state::from_queue(_download_queue, path),
			.selected       = (_last_selected  == path),
			.last_installed = (_last_installed == path),
			.installing     = _installing(),
		};

		s.sub_scope<Frame>(Id { version }, [&] (Scope<Vbox, Frame> &s) {
			s.attribute("style", "important");
			s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {

				s.widget(_hosted_image_main, attr);

				if (path == _last_selected && attr.changelog)
					s.widget(Hosted<Vbox, Frame, Vbox, Changelog> { Id { "changes" } },
					         image);
			});
		});
	}

	Hosted<Vbox, Frame, Vbox, Float, Operation_button> _check { Id { "check" } };

	Software_update_widget(Build_info           const &build_info,
	                       Nic_state            const &nic_state,
	                       Download_queue       const &download_queue,
	                       Index_update_queue   const &index_update_queue,
	                       File_operation_queue const &file_operation_queue,
	                       Depot_users          const &depot_users)
	:
		_build_info(build_info), _nic_state(nic_state),
		_download_queue(download_queue),
		_index_update_queue(index_update_queue),
		_file_operation_queue(file_operation_queue),
		_users(Id { "users" }, depot_users, _build_info.depot_user)
	{ }

	void view(Scope<Vbox> &s, Xml_node const &image_index) const
	{
		/* use empty ID to not interfere with matching the version in 'click'*/
		s.sub_scope<Frame>(Id { }, [&] (Scope<Vbox, Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {

				s.widget(_users);

				Depot_users_widget::User_properties const
					properties = _users.selected_user_properties();

				bool const offer_index_update = _users.one_selected()
				                             && _nic_state.ready()
				                             && properties.download_url;
				if (!offer_index_update)
					return;

				s.sub_scope<Small_vgap>();
				s.sub_scope<Float>([&] (Scope<Vbox, Frame, Vbox, Float> &s) {
					s.widget(_check, _index_update_in_progress(),
					         properties.public_key ? "Check for Updates"
					                               : "Check for unverified Updates");
				});
				s.sub_scope<Small_vgap>();
			});
		});

		image_index.for_each_sub_node("user", [&] (Xml_node const &user) {
			if (user.attribute_value("name", User()) == _users.selected())
				user.for_each_sub_node("image", [&] (Xml_node const &image) {
					_view_image_entry(s, image); }); });
	}

	struct Action : virtual Depot_users_widget::Action
	{
		virtual void query_image_index     (User const &) = 0;
		virtual void trigger_image_download(Path const &, Verify) = 0;
		virtual void update_image_index    (User const &, Verify) = 0;
		virtual void install_boot_image    (Path const &) = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		_users.propagate(at, action, [&] (User const &selected_user) {
			action.query_image_index(selected_user); });

		Verify const verify { _users.selected_user_properties().public_key };

		if (!_index_update_in_progress())
			_check.propagate(at, [&] {
				action.update_image_index(_users.selected(), verify); });

		Id   const version = at.matching_id<Vbox, Frame>();
		Path const path    = _image_path(version.value);

		if (version.valid()) {
			_last_selected = path;

			_hosted_image_main.propagate(at,
				[&] /* install */ {
					if (!_installing()) {
						action.install_boot_image(path);
						_last_installed = path;
					}
				},
				[&] /* download */ {
					action.trigger_image_download(path, verify);
				});
		}
	}

	bool keyboard_needed() const { return _users.keyboard_needed(); }

	void handle_key(Codepoint c, Action &action) { _users.handle_key(c, action); }

	void sanitize_user_selection() { _users.sanitize_unfold_state(); }

	void reset()
	{
		_last_installed = { };
		_last_selected  = { };
	}
};

#endif /* _VIEW__SOFTWARE_UPDATE_WIDGET_H_ */
