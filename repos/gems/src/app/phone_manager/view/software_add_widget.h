/*
 * \brief  Dialog for adding software components
 * \author Norman Feske
 * \date   2023-03-21
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_ADD_WIDGET_H_
#define _VIEW__SOFTWARE_ADD_WIDGET_H_

#include <model/build_info.h>
#include <model/nic_state.h>
#include <model/index_update_queue.h>
#include <model/index_menu.h>
#include <view/depot_users_widget.h>
#include <view/index_menu_widget.h>
#include <view/index_pkg_widget.h>
#include <view/component_add_widget.h>

namespace Sculpt { struct Software_add_widget; }


struct Sculpt::Software_add_widget : Widget_interface<Vbox>
{
	using Depot_users       = Depot_users_widget::Depot_users;
	using User              = Depot_users_widget::User;
	using Url               = Depot_users_widget::Url;
	using User_properties   = Depot_users_widget::User_properties;
	using Index             = Index_menu_widget::Index;
	using Construction_info = Component::Construction_info;

	Build_info         const  _build_info;
	Sculpt_version     const  _sculpt_version;
	Nic_state          const &_nic_state;
	Index_update_queue const &_index_update_queue;
	Download_queue     const &_download_queue;
	Construction_info  const &_construction_info;
	Depot_users        const &_depot_users;

	Hosted<Vbox, Frame, Vbox, Depot_users_widget>          _users;
	Hosted<Vbox, Float, Frame, Vbox, Index_menu_widget>    _menu;
	Hosted<Vbox, Float, Frame, Vbox, Component_add_widget> _component_add;

	using Hosted_pkg_widget = Hosted<Vbox, Index_pkg_widget>;

	Hosted_pkg_widget _pkg;

	Path _index_path() const { return Path(_users.selected(), "/index/", _sculpt_version); }

	bool _index_update_in_progress() const
	{
		using Update = Index_update_queue::Update;

		bool result = false;
		_index_update_queue.with_update(_index_path(), [&] (Update const &update) {
			if (update.active())
				result = true; });

		return result;
	}

	Software_add_widget(Build_info         const &build_info,
	                    Sculpt_version     const &sculpt_version,
	                    Nic_state          const &nic_state,
	                    Index_update_queue const &index_update_queue,
	                    Index              const &index,
	                    Download_queue     const &download_queue,
	                    Runtime_config     const &runtime_config,
	                    Construction_info  const &construction_info,
	                    Depot_users        const &depot_users)
	:
		_build_info(build_info), _sculpt_version(sculpt_version),
		_nic_state(nic_state), _index_update_queue(index_update_queue),
		_download_queue(download_queue), _construction_info(construction_info),
		_depot_users(depot_users),
		_users(Id { "users" }, depot_users, _build_info.depot_user),
		_menu(Id { "menu" }, index),
		_component_add(Id { "add" }, runtime_config),
		_pkg(Id { "pkg" })
	{ }

	struct Index_menu_entry
	{
		Download_queue     const &download_queue;
		Construction_info  const &construction_info;
		Hosted_pkg_widget  const &pkg;
		Depot_users_widget const &users;
		Nic_state          const &nic_state;

		using Hosted_entry = Hosted<Vbox, Menu_entry>;

		void view(Scope<Vbox> &s, Id const &id, auto const &text, Path const &pkg_path)
		{
			bool pkg_selected   = false;
			bool pkg_installing = false;

			String<100> label { text };

			if (pkg_path.length() > 1) {
				pkg_installing = download_queue.in_progress(pkg_path);

				construction_info.with_construction([&] (Component const &component) {
					if (component.path == pkg_path)
						pkg_selected = true; });

				label = { Pretty(label), "(", Depot::Archive::version(pkg_path), ")",
				          pkg_installing ? " installing... " : "... " };
			}

			s.widget(Hosted_entry { id }, pkg_selected, label);

			if (pkg_selected && !pkg_installing)
				construction_info.with_construction([&] (Component const &component) {
					s.widget(pkg, component, users.selected_user_properties(),
					         nic_state); });
		}
	};

	bool _component_add_widget_visible() const
	{
		bool result = false;
		if (_menu.pkg_selected())
			_construction_info.with_construction([&] (Component const &component) {
				if (component.blueprint_info.ready_to_deploy())
					result = true; });
		return result;
	}

	Hosted<Vbox, Frame, Vbox, Float, Operation_button> _check { Id { "check" } };

	void view(Scope<Vbox> &s) const
	{
		s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {
				s.widget(_users);

				User_properties const properties = _users.selected_user_properties();

				bool const offer_index_update = _users.one_selected()
				                             && _menu.top_level()
				                             && _nic_state.ready()
				                             && properties.download_url;

				if (!offer_index_update)
					return;

				s.sub_scope<Small_vgap>();
				s.sub_scope<Float>([&] (Scope<Vbox, Frame, Vbox, Float> &s) {
					s.widget(_check, _index_update_in_progress(),
					         properties.public_key ? "Update Index"
					                               : "Update unverified Index");
				});
				s.sub_scope<Small_vgap>();
			});
		});

		if (_users.unfolded())
			return;

		s.sub_scope<Vgap>();

		User const user = _users.selected();
		if (!_component_add_widget_visible() && !_menu.anything_visible(user))
			return;

		s.sub_scope<Float>([&] (Scope<Vbox, Float> &s) {
			s.sub_scope<Frame>([&] (Scope<Vbox, Float, Frame> &s) {
				s.sub_scope<Vbox>([&] (Scope<Vbox, Float, Frame, Vbox> &s) {
					s.sub_scope<Min_ex>(35);

					if (_component_add_widget_visible())
						_construction_info.with_construction([&] (Component const &component) {
							s.widget(_component_add, component); });

					else {
						s.widget(_menu, user,
							[&] (Scope<Vbox> &s, auto &&... args)
							{
								Index_menu_entry entry {
									.download_queue    = _download_queue,
									.construction_info = _construction_info,
									.pkg               = _pkg,
									.users             = _users,
									.nic_state         = _nic_state };

								entry.view(s, args...);
							});
					}
				});
			});
		});
	}

	void _reset_menu() { _menu.reset(); }

	bool keyboard_needed() const { return _users.keyboard_needed(); }

	struct Action : virtual Depot_users_widget::Action,
	                virtual Component::Construction_action
	{
		virtual void query_index        (User const &) = 0;
		virtual void update_sculpt_index(User const &, Verify) = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		_users.propagate(at, action, [&] (User const &selected_user) {
			action.query_index(selected_user);
			_reset_menu();
		});

		Verify const verify { _users.selected_user_properties().public_key };

		if (_component_add_widget_visible()) {
			_component_add.propagate(at, action,
				[&] /* leave */ {
					action.discard_construction();
					_menu.one_level_back();
				}
			);

		} else {

			_menu.propagate(at, _users.selected(),

				[&] /* enter pkg */ (Xml_node const &item) {

					auto path = item.attribute_value("path", Component::Path());
					auto info = item.attribute_value("info", Component::Info());

					action.new_construction(path, verify, info);
				},

				[&] /* leave pkg */ { action.discard_construction(); },

				[&] /* pkg operation */ (Clicked_at const &at) {
					_pkg.propagate(at); }
			);
		}

		if (!_index_update_in_progress())
			_check.propagate(at, [&] {
				action.update_sculpt_index(_users.selected(), verify); });
	}

	void clack(Clacked_at const &at, Action &action)
	{
		if (_component_add_widget_visible())
			_component_add.propagate(at, [&] /* launch */ {
				action.launch_construction();
				_reset_menu(); });

		_menu.propagate(at, [&] /* pkg operation */ (Clacked_at const &at) {
			_pkg.propagate(at, [&] { action.trigger_pkg_download(); }); });
	}

	void handle_key(Codepoint c, Action &action)
	{
		_users.handle_key(c, action);
	}

	void sanitize_user_selection() { _users.sanitize_unfold_state(); }
};

#endif /* _VIEW__SOFTWARE_ADD_WIDGET_H_ */
