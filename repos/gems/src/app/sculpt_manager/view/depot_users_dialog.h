/*
 * \brief  Dialog for selecting a depot user
 * \author Norman Feske
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DEPOT_USERS_DIALOG_H_
#define _VIEW__DEPOT_USERS_DIALOG_H_

#include <view/dialog.h>
#include <view/text_entry_field.h>
#include <model/depot_url.h>

namespace Sculpt { struct Depot_users_dialog; }


struct Sculpt::Depot_users_dialog
{
	public:

		using Depot_users  = Attached_rom_dataspace;
		using User         = Depot::Archive::User;
		using Url          = Depot_url::Url;
		using Hover_result = Hoverable_item::Hover_result;

		struct Action : Interface
		{
			virtual void add_depot_url(Depot_url const &depot_url) = 0;
		};

	private:

		using Url_edit_field = Text_entry_field<50>;

		Depot_users const &_depot_users;

		Action &_action;

		User _selected;

		bool _unfolded = false;

		Hoverable_item _user   { };
		Hoverable_item _button { };

		Url const _orig_edit_url { "https://" };

		Url_edit_field _url_edit_field { _orig_edit_url };

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

		static void _gen_vspacer(Xml_generator &xml, char const *name)
		{
			gen_named_node(xml, "label", name, [&] () {
				xml.attribute("text", " ");
				xml.attribute("font", "annotation/regular");
			});
		}

		static inline char const *_add_id() { return "/add"; }

		template <typename GEN_LABEL_FN, typename RIGHT_FN>
		void _gen_item(Xml_generator &xml, User const &name,
		               GEN_LABEL_FN const &gen_label_fn,
		               RIGHT_FN     const &right_fn) const
		{
			bool const selected = (name == _selected);

			gen_named_node(xml, "hbox", name, [&] () {
				gen_named_node(xml, "float", "left", [&] () {
					xml.attribute("west", "yes");
					xml.node("hbox", [&] () {
						gen_named_node(xml, "float", "button", [&] () {
							gen_named_node(xml, "button", "button", [&] () {

								_user.gen_hovered_attr(xml, name);

								if (selected)
									xml.attribute("selected", "yes");

								xml.attribute("style", "radio");
								xml.node("hbox", [&] () { });
							});
						});
						gen_named_node(xml, "label", "name", [&] () {
							gen_label_fn(); });
					});
				});
				gen_named_node(xml, "hbox", "right", [&] () {
					right_fn(); });
			});
		}

		void _gen_entry(Xml_generator &xml, Xml_node const user, bool last) const
		{
			User const name     = user.attribute_value("name", User());
			bool const selected = (name == _selected);
			Url  const url      = _url(user);
			Url  const label    = Depot_url::from_string(url).valid() ? url : Url(name);

			if (!selected && !_unfolded)
				return;

			_gen_item(xml, name,
				[&] /* label */ { xml.attribute("text", Path(" ", label)); },
				[&] /* right */ { }
			);

			if (_unfolded && !last)
				_gen_vspacer(xml, String<64>("below ", name).string());
		}

		Depot_url _depot_url(Xml_node const &depot_users) const
		{
			Depot_url const result =
				Depot_url::from_string(Depot_url::Url { _url_edit_field });

			/* check for duplicated user name */
			bool unique = true;
			depot_users.for_each_sub_node("user", [&] (Xml_node user) {
				User const name = user.attribute_value("name", User());
				if (name == result.user)
					unique = false;
			});

			return unique ? result : Depot_url { };
		}

		void _gen_add_entry(Xml_generator &xml, Xml_node const &depot_users) const
		{
			_gen_item(xml, _add_id(),

				[&] /* label */ {
					xml.attribute("text", Depot_url::Url(" ", _url_edit_field));
					xml.attribute("min_ex", 30);
					xml.node("cursor", [&] () {
						xml.attribute("at", _url_edit_field.cursor_pos + 1); });
				},

				[&] /* right */ {
					gen_named_node(xml, "float", "actions", [&] {
						xml.attribute("east", "yes");
						bool const editing = (_selected == _add_id());
						if (editing) {
							bool const url_valid = _depot_url(depot_users).valid();
							gen_named_node(xml, "button", "add", [&] {
								if (!url_valid)
									xml.attribute("style", "unimportant");
								xml.node("label", [&] {
									if (!url_valid)
										xml.attribute("style", "unimportant");
									xml.attribute("text", "Add"); });
							});
						} else {
							gen_named_node(xml, "button", "edit", [&] {
								xml.node("label", [&] {
									xml.attribute("text", "Edit"); }); });
						}
					});
				}
			);
		}

		void _gen_selection(Xml_generator &xml) const
		{
			Xml_node const depot_users = _depot_users.xml();

			size_t remain_count = depot_users.num_sub_nodes();

			remain_count++; /* account for '_gen_add_entry' */

			bool known_pubkey = false;

			gen_named_node(xml, "frame", "user_selection", [&] () {
				xml.node("vbox", [&] () {
					depot_users.for_each_sub_node("user", [&] (Xml_node user) {

						if (_selected == user.attribute_value("name", User()))
							known_pubkey = user.attribute_value("known_pubkey", false);

						bool const last = (--remain_count == 0);
						_gen_entry(xml, user, last); });

					if (_unfolded)
						_gen_add_entry(xml, depot_users);
				});
			});

			if (!_unfolded && !known_pubkey) {
				gen_named_node(xml, "button", "pubkey warning", [&] {
					xml.attribute("style", "invisible");
					xml.node("label", [&] {
						xml.attribute("font", "annotation/regular");
						xml.attribute("text", "missing public key for verification"); });
				});
			}
		}

	public:

		Depot_users_dialog(Depot_users const &depot_users,
		                   User        const &default_user,
		                   Action            &action)
		:
			_depot_users(depot_users), _action(action), _selected(default_user)
		{ }

		User selected() const
		{
			return (_selected == _add_id()) ? User() : _selected;
		}

		void generate(Xml_generator &xml) const { _gen_selection(xml); }

		bool unfolded() const { return _unfolded; }

		struct User_properties
		{
			bool exists;
			bool download_url;
			bool public_key;
		};

		User_properties selected_user_properties() const
		{
			User_properties result { };
			_depot_users.xml().for_each_sub_node([&] (Xml_node const &user) {
				if (_selected == user.attribute_value("name", User())) {
					result = {
						.exists       = true,
						.download_url = Depot_url::from_string(_url(user)).valid(),
						.public_key   = user.attribute_value("known_pubkey", false)
					};
				}
			});
			return result;
		}

		template <typename SELECT_FN>
		void click(SELECT_FN const &select_fn)
		{
			/* unfold depot users */
			if (!_unfolded) {
				_unfolded = true;
				return;
			}

			/* handle click on unfolded depot-user selection */

			auto select_depot_user = [&] (User const &user)
			{
				_selected = user;
				select_fn(user);
				_unfolded = false;
				_url_edit_field = _orig_edit_url;
			};

			if (_user._hovered.length() <= 1)
				return;

			if (_user.hovered(_add_id())) {
				if (_button.hovered("add")) {
					Depot_url const depot_url = _depot_url(_depot_users.xml());
					if (depot_url.valid()) {
						_action.add_depot_url(depot_url);
						select_depot_user(depot_url.user);
					}
				} else {
					_selected = _add_id();
				}
			} else {
				select_depot_user(_user._hovered);
			}
		}

		Hover_result hover(Xml_node const &hover)
		{
			return Dialog::any_hover_changed(
				_user  .match(hover, "frame", "vbox", "hbox", "name"),
				_button.match(hover, "frame", "vbox", "hbox", "hbox", "float", "button", "name")
			);
		}

		void reset_hover() { _user._hovered = { }; }

		bool hovered() const { return _user._hovered.valid();  }

		bool keyboard_needed() const { return _selected == _add_id(); }

		void handle_key(Codepoint c)
		{
			if (_selected != _add_id())
				return;

			/* prevent input of printable yet risky characters as URL */
			if (c.value == ' ' || c.value == '"')
				return;

			_url_edit_field.apply(c);
		}

		bool one_selected() const { return !_unfolded && _selected.length() > 1; }
};

#endif /* _VIEW__DEPOT_USERS_DIALOG_H_ */
