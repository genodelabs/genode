/*
 * \brief  Widget for selecting a depot user
 * \author Norman Feske
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DEPOT_USERS_WIDGET_H_
#define _VIEW__DEPOT_USERS_WIDGET_H_

#include <view/dialog.h>
#include <view/text_entry_field.h>
#include <model/depot_url.h>

namespace Sculpt { struct Depot_users_widget; }


struct Sculpt::Depot_users_widget : Widget<Vbox>
{
	public:

		using Depot_users = Rom_data;
		using User        = Depot::Archive::User;
		using Url         = Depot_url::Url;

		struct Action : Interface
		{
			virtual void add_depot_url(Depot_url const &depot_url) = 0;
		};

	private:

		using Url_edit_field = Text_entry_field<50>;

		User const _default_user;

		Depot_users const &_depot_users;

		User _selected { _default_user };

		bool _unfolded = false;

		bool _selected_user_exists = false;

		Url _url(Xml_node const &user) const
		{
			if (!user.has_sub_node("url"))
				return { };

			Url const url = user.sub_node("url").decoded_content<Url>();

			/*
			 * Ensure that the URL does not contain any '"' character because
			 * it will be taken as an XML attribute value.
			 */
			for (char const *s = url.string(); *s; s++)
				if (*s == '"')
					return { };

			User const name = user.attribute_value("name", User());

			return Url(url, "/", name);
		}

		static inline char const *_add_id() { return "/add"; }

		struct Conditional_button : Widget<Button>
		{
			Dialog::Event::Seq_number _seq_number { };

			void view(Scope<Button> &s, bool ready, auto const &text) const
			{
				bool const selected = _seq_number == s.hover.seq_number;

				if (!ready)
					s.attribute("style", "unimportant");

				if (selected)
					s.attribute("selected", "yes");

				if (s.hovered() && !s.dragged() && !selected && ready)
					s.attribute("hovered",  "yes");

				s.sub_scope<Label>(text, [&] (auto &s) {
					if (!ready)
						s.attribute("style", "unimportant"); });
			}

			void view(Scope<Button> &s, bool ready) const
			{
				view(s, ready, s.id.value);
			}

			void click(Clicked_at const &at, auto const &fn)
			{
				_seq_number = at.seq_number;
				fn();
			}
		};

		struct Item : Widget<Hbox>
		{
			void view(Scope<Hbox> &s, bool selected, auto const &text) const
			{
				bool const hovered = s.hovered();

				s.sub_scope<Left_floating_hbox>([&] (Scope<Hbox, Left_floating_hbox> &s) {
					s.sub_scope<Icon>("radio", Icon::Attr { .hovered  = hovered,
					                                        .selected = selected });
					s.sub_scope<Label>(text);
				});
			}
		};

		struct Edit_item : Widget<Hbox>
		{
			Url const _orig_edit_url { "https://" };

			Url_edit_field _url_edit_field { _orig_edit_url };

			Depot_url depot_url(Xml_node const &depot_users) const
			{
				Depot_url const result =
					Depot_url::from_string(Depot_url::Url { _url_edit_field });

				/* check for duplicated user name */
				bool unique = true;
				depot_users.for_each_sub_node("user", [&] (Xml_node user) {
					User const name = user.attribute_value("name", User());
					if (name == result.user)
						unique = false; });

				return unique ? result : Depot_url { };
			}

			Hosted<Hbox, Hbox, Float, Conditional_button> _add  { Id { "Add" } };
			Hosted<Hbox, Hbox, Float, Action_button>      _edit { Id { "Edit" } };

			bool _ready_to_add(Xml_node const &depot_users) const
			{
				return depot_url(depot_users).valid();
			}

			void view(Scope<Hbox> &s, bool selected, Xml_node const &depot_users) const
			{
				bool const hovered = s.hovered() && !s.dragged() && !selected;

				s.sub_scope<Left_floating_hbox>([&] (Scope<Hbox, Left_floating_hbox> &s) {

					s.sub_scope<Icon>("radio", Icon::Attr { .hovered  = hovered,
					                                        .selected = selected });

					auto const text = Depot_url::Url(" ", _url_edit_field);
					s.sub_scope<Label>(text, [&] (auto &s) {
						s.attribute("min_ex", 30);
						s.template sub_node("cursor", [&] {
							s.attribute("at", _url_edit_field.cursor_pos + 1); });
					});
				});

				s.sub_scope<Hbox>([&] (Scope<Hbox, Hbox> &s) {
					s.sub_scope<Float>([&] (Scope<Hbox, Hbox, Float> &s) {
						s.attribute("east", "yes");
						if (selected)
							s.widget(_add, _ready_to_add(depot_users));
						else
							s.widget(_edit);
					});
				});
			}

			void reset()
			{
				_url_edit_field = _orig_edit_url;
			}

			void handle_key(Codepoint c, auto const &enter_fn)
			{
				/* prevent input of printable yet risky characters as URL */
				if (c.value == ' ' || c.value == '"')
					return;

				/* respond to enter key */
				if (c.value == 10)
					enter_fn();

				_url_edit_field.apply(c);
			}

			void click(Clicked_at const &, Xml_node const &depot_users, auto const add_fn)
			{
				if (_ready_to_add(depot_users))
					add_fn();
			}
		};

		Hosted<Vbox, Frame, Vbox, Edit_item> _edit_item { Id { _add_id() } };

	public:

		Depot_users_widget(Depot_users const &depot_users,
		                   User        const &default_user)
		:
			_default_user(default_user), _depot_users(depot_users)
		{ }

		User selected() const
		{
			return (_selected == _add_id()) ? User() : _selected;
		}

		void _view(Scope<Vbox> &s, Xml_node const &depot_users) const
		{
			bool known_pubkey = false;

			s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
				s.sub_scope<Vbox>([&] (Scope<Vbox, Frame, Vbox> &s) {
					depot_users.for_each_sub_node("user", [&] (Xml_node const &user) {

						if (_selected == user.attribute_value("name", User()))
							known_pubkey = user.attribute_value("known_pubkey", false);

						User const name     = user.attribute_value("name", User());
						bool const selected = (name == _selected);
						Url  const url      = _url(user);
						Url  const label    = Depot_url::from_string(url).valid() ? url : Url(name);
						bool const show_all = _unfolded || !_selected_user_exists;

						if (!selected && !show_all)
							return;

						Hosted<Vbox, Frame, Vbox, Item> item { Id { name } };

						s.widget(item, selected, Path(" ", label));
					});

					if (_unfolded || !_selected_user_exists)
						s.widget(_edit_item, (_selected == _add_id()), depot_users);
				});
			});

			if (!_unfolded && !known_pubkey && _selected_user_exists) {
				s.sub_scope<Button>([&] (Scope<Vbox, Button> &s) {
					s.attribute("style", "invisible");
					s.sub_scope<Annotation>("missing public key for verification");
				});
			}
		}

		void view(Scope<Vbox> &s) const
		{
			_depot_users.with_xml([&] (Xml_node const &depot_users) {
				_view(s, depot_users); });
		}

		bool unfolded() const { return _unfolded || !_selected_user_exists; }

		struct User_properties
		{
			bool exists;
			bool download_url;
			bool public_key;
		};

		User_properties selected_user_properties() const
		{
			User_properties result { };
			_depot_users.with_xml([&] (Xml_node const &users) {
				users.for_each_sub_node([&] (Xml_node const &user) {
					if (_selected == user.attribute_value("name", User())) {
						result = {
							.exists       = true,
							.download_url = Depot_url::from_string(_url(user)).valid(),
							.public_key   = user.attribute_value("known_pubkey", false)
						};
					}
				});
			});
			return result;
		}

		void _select_depot_user(User const &user)
		{
			_selected = user;
			_unfolded = false;
			_selected_user_exists = true;
			_edit_item.reset();
		}

		void _add_and_select_new_depot_user(Action &action)
		{
			_depot_users.with_xml([&] (Xml_node const &users) {
				Depot_url const depot_url = _edit_item.depot_url(users);
				if (depot_url.valid()) {
					action.add_depot_url(depot_url);
					_select_depot_user(depot_url.user);
				}
			});
		}

		void click(Clicked_at const &at, Action &action, auto const &select_fn)
		{
			/* unfold depot users */
			if (!_unfolded) {
				_unfolded = true;
				return;
			}

			Id const item = at.matching_id<Vbox, Frame, Vbox, Item>();
			if (item.valid()) {
				if (item.value == _add_id()) {
					_selected = item.value;
				} else {
					_select_depot_user(item.value);
					select_fn(item.value);
				}
			}

			_depot_users.with_xml([&] (Xml_node const &users) {
				_edit_item.propagate(at, users, [&] {
					_add_and_select_new_depot_user(action); }); });
		}

		bool keyboard_needed() const { return _selected == _add_id(); }

		void handle_key(Codepoint c, Action &action)
		{
			if (_selected == _add_id())
				_edit_item.handle_key(c, [&] {
					_add_and_select_new_depot_user(action); });
		}

		bool one_selected() const { return !_unfolded && _selected.length() > 1; }

		void sanitize_unfold_state()
		{
			/*
			 * If the selected depot user does not exist in the depot, show
			 * list of available users.
			 */
			_selected_user_exists = false;

			_depot_users.with_xml([&] (Xml_node const &users) {
				users.for_each_sub_node([&] (Xml_node const &user) {
					if (_selected == user.attribute_value("name", User()))
						_selected_user_exists = true; }); });
		}
};

#endif /* _VIEW__DEPOT_USERS_WIDGET_H_ */
