/*
 * \brief  Popup dialog
 * \author Norman Feske
 * \date   2018-09-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__POPUP_DIALOG_H_
#define _VIEW__POPUP_DIALOG_H_

/* local includes */
#include <types.h>
#include <model/launchers.h>
#include <model/sculpt_version.h>
#include <model/component.h>
#include <model/runtime_config.h>
#include <model/download_queue.h>
#include <model/nic_state.h>
#include <model/index_menu.h>
#include <view/dialog.h>
#include <view/pd_route_widget.h>
#include <view/resource_widget.h>
#include <view/debug_widget.h>
#include <depot_query.h>

namespace Sculpt { struct Popup_dialog; }


struct Sculpt::Popup_dialog : Dialog::Top_level_dialog
{
	using Depot_users    = Attached_rom_dataspace;
	using Blueprint_info = Component::Blueprint_info;

	Env &_env;

	Sculpt_version const _sculpt_version { _env };

	Launchers  const &_launchers;
	Nic_state  const &_nic_state;
	Nic_target const &_nic_target;

	bool _nic_ready() const { return _nic_target.ready() && _nic_state.ready(); }

	Runtime_info   const &_runtime_info;
	Runtime_config const &_runtime_config;
	Download_queue const &_download_queue;
	Depot_users    const &_depot_users;

	Depot_query &_depot_query;

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

	struct Refresh : Interface, Noncopyable
	{
		virtual void refresh_popup_dialog() = 0;
	};

	Refresh &_refresh;

	struct Action : Interface
	{
		virtual void launch_global(Path const &launcher) = 0;

		virtual Start_name new_construction(Component::Path const &pkg, Verify,
		                                    Component::Info const &info) = 0;

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

		virtual void trigger_download(Path const &, Verify) = 0;
		virtual void remove_index(Depot::Archive::User const &) = 0;
	};

	Action &_action;

	Construction_info const &_construction_info;

	using Route_entry   = Hosted<Frame, Vbox, Frame, Vbox, Menu_entry>;
	using Service_entry = Hosted<Frame, Vbox, Frame, Vbox, Menu_entry>;

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

		void click(Clicked_at const &, auto const &fn) { fn(); }
	};

	Hosted<Frame, Vbox, Sub_menu_title>               _back      { Id { "back" } };
	Hosted<Frame, Vbox, Deferred_action_button>       _launch    { Id { "Add component" } };
	Hosted<Frame, Vbox, Float, Vbox, Float, Deferred_action_button> _install { Id { "install" } };
	Hosted<Frame, Vbox, Frame, Vbox, Resource_widget> _resources { Id { "resources" } };
	Hosted<Frame, Vbox, Frame, Pd_route_widget>       _pd_route  { Id { "pd_route" }, _runtime_config };
	Hosted<Frame, Vbox, Frame, Debug_widget>          _debug     { Id { "debug" } };

	enum State { TOP_LEVEL, DEPOT_REQUESTED, DEPOT_SHOWN, DEPOT_SELECTION,
	             INDEX_REQUESTED, INDEX_SHOWN,
	             PKG_REQUESTED, PKG_SHOWN, ROUTE_SELECTED };

	State _state { TOP_LEVEL };

	typedef Depot::Archive::User User;
	User _selected_user { };

	Blueprint_info _blueprint_info { };

	Component::Name _construction_name { };

	Id _selected_route { };

	bool _route_selected(Route::Id const &id) const
	{
		return _selected_route.valid() && id == _selected_route.value;
	}

	bool _resource_dialog_selected() const
	{
		return _route_selected("resources");
	}

	void _apply_to_selected_route(Action &action, auto const &fn)
	{
		unsigned cnt = 0;
		action.apply_to_construction([&] (Component &component) {
			component.routes.for_each([&] (Route &route) {
				if (_route_selected(Route::Id(cnt++)))
					fn(route); }); });
	}

	Index_menu _menu { };

	void depot_users_scan_updated()
	{
		if (_state == DEPOT_REQUESTED)
			_state = DEPOT_SHOWN;

		if (_state != TOP_LEVEL)
			_refresh.refresh_popup_dialog();
	}

	Attached_rom_dataspace _index_rom { _env, "report -> runtime/depot_query/index" };

	Signal_handler<Popup_dialog> _index_handler {
		_env.ep(), *this, &Popup_dialog::_handle_index };

	void _handle_index()
	{
		/* prevent modifications of index while browsing it */
		if (_state >= INDEX_SHOWN)
			return;

		_index_rom.update();

		if (_state == INDEX_REQUESTED)
			_state = INDEX_SHOWN;

		_refresh.refresh_popup_dialog();
	}

	bool _index_avail(User const &user) const
	{
		bool result = false;
		_index_rom.xml().for_each_sub_node("index", [&] (Xml_node index) {
			if (index.attribute_value("user", User()) == user)
				result = true; });
		return result;
	};

	Path _index_path(User const &user) const
	{
		return Path(user, "/index/", _sculpt_version);
	}

	template <typename FN>
	void _for_each_menu_item(FN const &fn) const
	{
		_menu.for_each_item(_index_rom.xml(), _selected_user, fn);
	}

	void _view_pkg_elements (Scope<Frame, Vbox> &, Component const &) const;
	void _view_menu_elements(Scope<Frame, Vbox> &, Xml_node const &depot_users) const;

	void view(Scope<> &s) const override
	{
		s.sub_scope<Frame>([&] (Scope<Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
				_view_menu_elements(s, _depot_users.xml()); }); });
	}

	void click(Clicked_at const &) override;

	void clack(Clacked_at const &at) override
	{
		_launch.propagate(at, [&] {
			_action.launch_construction();
			reset();
		});

		_install.propagate(at, [&] {
			bool const pkg_need_install = !_blueprint_info.pkg_avail
			                            || _blueprint_info.incomplete();
			if (pkg_need_install) {
				_construction_info.with_construction([&] (Component const &component) {
					_action.trigger_download(component.path, component.verify);
					_refresh.refresh_popup_dialog();
				});
			}
		});
	}

	void drag(Dragged_at const &) override { }

	void reset()
	{
		_state = TOP_LEVEL;
		_selected_user = User();
		_selected_route = { };
		_menu._level = 0;
	}

	Popup_dialog(Env &env, Refresh &refresh, Action &action,
	             Launchers         const &launchers,
	             Nic_state         const &nic_state,
	             Nic_target        const &nic_target,
	             Runtime_info      const &runtime_info,
	             Runtime_config    const &runtime_config,
	             Download_queue    const &download_queue,
	             Depot_users       const &depot_users,
	             Depot_query             &depot_query,
	             Construction_info const &construction_info)
	:
		Top_level_dialog("popup"),
		_env(env), _launchers(launchers),
		_nic_state(nic_state), _nic_target(nic_target),
		_runtime_info(runtime_info), _runtime_config(runtime_config),
		_download_queue(download_queue), _depot_users(depot_users),
		_depot_query(depot_query), _refresh(refresh), _action(action),
		_construction_info(construction_info)
	{
		_index_rom.sigh(_index_handler);
	}

	bool depot_query_needs_users() const { return _state >= TOP_LEVEL; }

	void gen_depot_query(Xml_generator &xml, Xml_node const &depot_users) const
	{
		if (_state >= TOP_LEVEL)
			depot_users.for_each_sub_node("user", [&] (Xml_node user) {
				xml.node("index", [&] {
					User const name = user.attribute_value("name", User());
					xml.attribute("user",    name);
					xml.attribute("version", _sculpt_version);
					if (_state >= INDEX_REQUESTED && _selected_user == name)
						xml.attribute("content", "yes");
				});
			});

		if (_state >= PKG_REQUESTED)
			_construction_info.with_construction([&] (Component const &component) {
				xml.node("blueprint", [&] {
					xml.attribute("pkg", component.path); }); });
	}

	template <typename FN>
	void for_each_viewed_launcher(FN const &fn) const
	{
		_launchers.for_each([&] (Launchers::Info const &info) {
			if (_runtime_info.present_in_runtime(info.path))
				return;

			fn(info);
		});
	}

	void apply_blueprint(Component &construction, Xml_node blueprint)
	{
		if (_state < PKG_REQUESTED)
			return;

		construction.try_apply_blueprint(blueprint);

		_blueprint_info = construction.blueprint_info;

		if (_blueprint_info.ready_to_deploy() && _state == PKG_REQUESTED)
			_state = PKG_SHOWN;

		_refresh.refresh_popup_dialog();
	}

	bool interested_in_file_operations() const
	{
		return _state == DEPOT_SELECTION;
	}
};

#endif /* _VIEW__POPUP_DIALOG_H_ */
