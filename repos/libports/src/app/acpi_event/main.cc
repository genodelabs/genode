/*
 * \brief  Component reading the reported ACPI ROMs and transforming it
 *         to Genode Input events. The actual mapping must be configured
 *         explicitly externally.
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <util/avl_tree.h>

/* os includes */
#include <event_session/connection.h>

namespace Transform {

	using namespace Genode;

	struct Main;
	class Keys;
}


class Transform::Keys : public Avl_node<Transform::Keys>
{
	public:

		enum Type { PRESS_RELEASE, PRESS, RELEASE };

	private:

		const Input::Keycode _code;
		const uint64_t       _acpi_value;
		uint64_t             _acpi_count = 0;
		bool                 _first = true;
		Type                 _type;

		Keys *_find_by_acpi_value(uint64_t acpi_value)
		{
			if (acpi_value == _acpi_value) return this;
			Keys *key = this->child(acpi_value > _acpi_value);
			return key ? key->_find_by_acpi_value(acpi_value) : nullptr;
		}

	public:

		Keys(Input::Keycode code, uint64_t acpi_value, Type type)
		: _code(code), _acpi_value(acpi_value), _type(type) { }

		Input::Keycode key_code() const { return _code; }

		bool higher(Keys *k) const { return k->_acpi_value > _acpi_value; }

		Type type() const { return _type; }

		uint64_t update_count(uint64_t acpi_count)
		{
			uint64_t diff = acpi_count > _acpi_count ?
			                acpi_count - _acpi_count : 0;

			/*
			 * If we get the first time this ACPI event, than we don't
			 * use the diff value, because it could be arbitrarily.
			 */
			if (_first) {
				_first = false;
				diff = 1;
			}

			_acpi_count = acpi_count;

			return diff;
		}

		static Keys * find_by(Avl_tree<Keys> &map, uint64_t acpi_code)
		{
			Keys * head = map.first();
			if (!head)
				return head;

			return head->_find_by_acpi_value(acpi_code);
		}
};


struct Transform::Main
{
	enum {
		ACPI_POWER_BUTTON, ACPI_LID_OPEN, ACPI_LID_CLOSED,
		ACPI_AC_ONLINE, ACPI_AC_OFFLINE, ACPI_BATTERY
	};

	Heap _heap;

	Attached_rom_dataspace _config;
	Attached_rom_dataspace _acpi_ac;
	Attached_rom_dataspace _acpi_battery;
	Attached_rom_dataspace _acpi_ec;
	Attached_rom_dataspace _acpi_fixed;
	Attached_rom_dataspace _acpi_lid;
	Attached_rom_dataspace _acpi_hid;

	Signal_handler<Main> _dispatch_acpi_ac;
	Signal_handler<Main> _dispatch_acpi_battery;
	Signal_handler<Main> _dispatch_acpi_ec;
	Signal_handler<Main> _dispatch_acpi_fixed;
	Signal_handler<Main> _dispatch_acpi_lid;
	Signal_handler<Main> _dispatch_acpi_hid;

	Event::Connection _event;

	Avl_tree<Keys> _map_ec      { };
	Avl_tree<Keys> _map_hid     { };
	Avl_tree<Keys> _map_special { };

	Main(Env &env)
	:
		_heap(env.ram(), env.rm()),
		_config(env, "config"),
		_acpi_ac(env, "acpi_ac"),
		_acpi_battery(env, "acpi_battery"),
		_acpi_ec(env, "acpi_ec"),
		_acpi_fixed(env, "acpi_fixed"),
		_acpi_lid(env, "acpi_lid"),
		_acpi_hid(env, "acpi_hid"),
		_dispatch_acpi_ac(env.ep(), *this, &Main::check_acpi_ac),
		_dispatch_acpi_battery(env.ep(), *this, &Main::check_acpi_battery),
		_dispatch_acpi_ec(env.ep(), *this, &Main::check_acpi_ec),
		_dispatch_acpi_fixed(env.ep(), *this, &Main::check_acpi_fixed),
		_dispatch_acpi_lid(env.ep(), *this, &Main::check_acpi_lid),
		_dispatch_acpi_hid(env.ep(), *this, &Main::check_acpi_hid),
		_event(env)
	{
		Xml_node config(_config.local_addr<char>(), _config.size());
		config.for_each_sub_node("map", [&] (Xml_node map_node) {
			try {
				uint64_t acpi_value = 0;
				String<8> acpi_type;
				String<8> acpi_value_string;
				Input::Key_name to_key;

				String<16> key_type("PRESS_RELEASE");
				Keys::Type press_release = Keys::Type::PRESS_RELEASE;

				map_node.attribute("acpi").value(acpi_type);
				map_node.attribute("to_key").value(to_key);
				try {
					map_node.attribute("as").value(key_type);
					if (key_type == "PRESS")
						press_release = Keys::Type::PRESS;
					else if (key_type == "RELEASE")
						press_release = Keys::Type::RELEASE;
					else if (key_type != "PRESS_RELEASE")
						throw 0;
				} catch (Xml_node::Nonexistent_attribute) { }

				if (acpi_type == "lid" || acpi_type == "ac") {
					map_node.attribute("value").value(acpi_value_string);

					if (acpi_type == "lid") {
						if (acpi_value_string == "OPEN")
							acpi_value = ACPI_LID_OPEN;
						else if (acpi_value_string == "CLOSED")
							acpi_value = ACPI_LID_CLOSED;
						else throw 1;
					} else
					if (acpi_type == "ac") {
						if (acpi_value_string == "ONLINE")
							acpi_value = ACPI_AC_ONLINE;
						else if (acpi_value_string == "OFFLINE")
							acpi_value = ACPI_AC_OFFLINE;
						else throw 2;
					} else
						throw 3;
				} else
					map_node.attribute("value").value(acpi_value);

				Input::Keycode key_code = Input::key_code(to_key);

				if (key_code == Input::Keycode::KEY_UNKNOWN)
					throw 4;

				if (acpi_type == "ec")
					_map_ec.insert(new (_heap) Keys(key_code, acpi_value,
					                                press_release));
				else if (acpi_type == "fixed")
					_map_special.insert(new (_heap) Keys(key_code,
					                                     ACPI_POWER_BUTTON,
					                                     press_release));
				else if (acpi_type == "lid" || acpi_type == "ac")
					_map_special.insert(new (_heap) Keys(key_code, acpi_value,
					                                     press_release));
				else if (acpi_type == "battery")
					_map_special.insert(new (_heap) Keys(key_code,
					                                     ACPI_BATTERY,
					                                     press_release));
				else if (acpi_type == "hid")
					_map_hid.insert(new (_heap) Keys(key_code, acpi_value,
					                                 press_release));
				else
					throw 5;
			} catch (...) {

				/* abort on malformed configuration */
				error("map item '", map_node, "' Aborting...");
				env.parent().exit(1);
			}
		});

		_acpi_ac.sigh(_dispatch_acpi_ac);
		_acpi_battery.sigh(_dispatch_acpi_battery);
		_acpi_ec.sigh(_dispatch_acpi_ec);
		_acpi_fixed.sigh(_dispatch_acpi_fixed);
		_acpi_lid.sigh(_dispatch_acpi_lid);
		_acpi_hid.sigh(_dispatch_acpi_hid);

		/* check for initial valid ACPI data */
		check_acpi_ac();
		check_acpi_battery();
		check_acpi_ec();
		check_acpi_fixed();
		check_acpi_lid();
		check_acpi_hid();
	}

	void _check_acpi(Attached_rom_dataspace &rom, Avl_tree<Keys> &map,
	                 char const * const name)
	{
		rom.update();

		if (!rom.valid()) return;

		Xml_node event(rom.local_addr<char>(), rom.size());

		event.for_each_sub_node(name, [&] (Xml_node const &node) {
			node.for_each_sub_node("data", [&] (Xml_node const &data_node) {
				try {
					uint64_t acpi_value = 0;
					uint64_t acpi_count = 0;

					data_node.attribute("value").value(acpi_value);
					data_node.attribute("count").value(acpi_count);

					Keys * key = Keys::find_by(map, acpi_value);
					if (!key)
						return;

					if (!key->update_count(acpi_count))
						return;

					submit_input(key);
				} catch (...) { /* be robust - ignore ill-formated ACPI data */ }
			});
		});
	}

	void check_acpi_ec() { _check_acpi(_acpi_ec, _map_ec, "ec"); }

	void check_acpi_hid() { _check_acpi(_acpi_hid, _map_hid, "hid"); }

	void submit_input(Keys * key)
	{
		_event.with_batch([&] (Event::Session_client::Batch &batch) {

			if (!key)
				return;

			if (key->type() == Keys::Type::PRESS_RELEASE ||
			    key->type() == Keys::Type::PRESS)
				batch.submit(Input::Press{key->key_code()});

			if (key->type() == Keys::Type::PRESS_RELEASE ||
			    key->type() == Keys::Type::RELEASE)
				batch.submit(Input::Release{key->key_code()});
		});
	}

	void check_acpi_fixed()
	{
		_acpi_fixed.update();

		if (!_acpi_fixed.valid()) return;

		Xml_node fixed_event(_acpi_fixed.local_addr<char>(), _acpi_fixed.size());

		fixed_event.for_each_sub_node("power_button", [&] (Xml_node pw_node) {
			try {
				bool pressed = false;
				uint64_t acpi_count = 0;

				pw_node.attribute("value").value(pressed);
				pw_node.attribute("count").value(acpi_count);

				Keys * key = Keys::find_by(_map_special, ACPI_POWER_BUTTON);
				if (!key)
					return;

				if (!key->update_count(acpi_count) && pressed)
					return;

				submit_input(key);
			} catch (...) { /* be robust - ignore ill-formated ACPI data */ }
		});
	}

	void check_acpi_battery()
	{
		_acpi_battery.update();

		if (!_acpi_battery.valid()) return;

		/* make use of it if we need to ... */
		Xml_node battery_node(_acpi_battery.local_addr<char>(),
		                      _acpi_battery.size());

		submit_input(Keys::find_by(_map_special, ACPI_BATTERY));
	}

	void check_acpi_ac()
	{
		_acpi_ac.update();

		if (!_acpi_ac.valid()) return;

		Xml_node ac_node(_acpi_ac.local_addr<char>(), _acpi_ac.size());

		_check_acpi(ac_node, "ac", ACPI_AC_ONLINE, ACPI_AC_OFFLINE);
	}

	void check_acpi_lid()
	{
		_acpi_lid.update();

		if (!_acpi_lid.valid()) return;

		Xml_node lid_node(_acpi_lid.local_addr<char>(), _acpi_lid.size());

		_check_acpi(lid_node, "lid", ACPI_LID_OPEN, ACPI_LID_CLOSED);
	}

	void _check_acpi(Xml_node &xml_node, const char * sub_name,
	                 const unsigned state_o, const unsigned state_c)
	{
		xml_node.for_each_sub_node(sub_name, [&] (Xml_node node) {
			try {
				uint64_t acpi_value = 0;
				uint64_t acpi_count = 0;

				node.attribute("value").value(acpi_value);
				node.attribute("count").value(acpi_count);

				enum { STATE_C = 0, STATE_O = 1 };

				Keys * key = nullptr;
				if (acpi_value == STATE_O)
					key = Keys::find_by(_map_special, state_o);
				if (acpi_value == STATE_C)
					key = Keys::find_by(_map_special, state_c);
				if (!key)
					return;

				if (!key->update_count(acpi_count))
					return;

				if (acpi_value == STATE_C)
					submit_input(key);

				if (acpi_value == STATE_O)
					submit_input(key);
			} catch (...) { /* be robust - ignore ill-formated ACPI data */ }
		});
	}
};


void Component::construct(Genode::Env &env) {
	static Transform::Main main(env); }
