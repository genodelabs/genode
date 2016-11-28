/*
 * \brief  Component reading the reported ACPI ROMs and transforming it
 *         to Genode Input events. The actual mapping must be configured
 *         explicitly externally.
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base includes */
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <util/avl_tree.h>

/* os includes */
#include <input/component.h>
#include <input/root.h>
#include <os/attached_rom_dataspace.h>

namespace Transform {
	using Genode::Xml_node;
	using Genode::String;

	struct Main;
	class Keys;
};


class Transform::Keys : public Genode::Avl_node<Transform::Keys> {

	public:

		enum Type { PRESS_RELEASE, PRESS, RELEASE };

	private:

		const Input::Keycode _code;
		const long           _acpi_value;
		unsigned long        _acpi_count = 0;
		bool                 _first = true;
		Type                 _type;

		static Genode::Avl_tree<Keys> _map_ec;
		static Genode::Avl_tree<Keys> _map_special;

		Keys *_find_by_acpi_value(long acpi_value)
		{
			if (acpi_value == _acpi_value) return this;
			Keys *key = this->child(acpi_value > _acpi_value);
			return key ? key->_find_by_acpi_value(acpi_value) : nullptr;
		}

	public:

		Keys(Input::Keycode code, long acpi_value, Type type)
		: _code(code), _acpi_value(acpi_value), _type(type) { }

		Input::Keycode key_code() const { return _code; }

		bool higher(Keys *k) const { return k->_acpi_value > _acpi_value; }

		Type const type() { return _type; }

		unsigned long update_count(unsigned long acpi_count)
		{
			unsigned long diff = acpi_count > _acpi_count ?
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

		static Keys * find_by_ec(long acpi_code)
		{
			Keys * head = _map_ec.first();
			if (!head)
				return head;

			return head->_find_by_acpi_value(acpi_code);
		}

		static Keys * find_by_fixed(long acpi_code)
		{
			Keys * head = _map_special.first();
			if (!head)
				return head;

			return head->_find_by_acpi_value(acpi_code);
		}

		static void insert_ec(Keys * key) { _map_ec.insert(key); }
		static void insert_special(Keys * key) { _map_special.insert(key); }
};


Genode::Avl_tree<Transform::Keys> Transform::Keys::_map_ec;
Genode::Avl_tree<Transform::Keys> Transform::Keys::_map_special;


struct Transform::Main {

	enum {
		ACPI_POWER_BUTTON, ACPI_LID_OPEN, ACPI_LID_CLOSED,
		ACPI_AC_ONLINE, ACPI_AC_OFFLINE, ACPI_BATTERY
	};

	Genode::Heap _heap;

	Genode::Attached_rom_dataspace _config;
	Genode::Attached_rom_dataspace _acpi_ac;
	Genode::Attached_rom_dataspace _acpi_battery;
	Genode::Attached_rom_dataspace _acpi_ec;
	Genode::Attached_rom_dataspace _acpi_fixed;
	Genode::Attached_rom_dataspace _acpi_lid;

	Genode::Signal_handler<Main> _dispatch_acpi_ac;
	Genode::Signal_handler<Main> _dispatch_acpi_battery;
	Genode::Signal_handler<Main> _dispatch_acpi_ec;
	Genode::Signal_handler<Main> _dispatch_acpi_fixed;
	Genode::Signal_handler<Main> _dispatch_acpi_lid;

	Input::Session_component _session;
	Input::Root_component    _root;

	Main(Genode::Env &env)
	:
		_heap(env.ram(), env.rm()),
		_config(env, "config"),
		_acpi_ac(env, "acpi_ac"),
		_acpi_battery(env, "acpi_battery"),
		_acpi_ec(env, "acpi_ec"),
		_acpi_fixed(env, "acpi_fixed"),
		_acpi_lid(env, "acpi_lid"),
		_dispatch_acpi_ac(env.ep(), *this, &Main::check_acpi_ac),
		_dispatch_acpi_battery(env.ep(), *this, &Main::check_acpi_battery),
		_dispatch_acpi_ec(env.ep(), *this, &Main::check_acpi_ec),
		_dispatch_acpi_fixed(env.ep(), *this, &Main::check_acpi_fixed),
		_dispatch_acpi_lid(env.ep(), *this, &Main::check_acpi_lid),
		_root(env.ep().rpc_ep(), _session)
	{
		Xml_node config(_config.local_addr<char>(), _config.size());
		config.for_each_sub_node("map", [&] (Xml_node map_node) {
			try {
				long acpi_value = 0;
				Genode::String<8> acpi_type;
				Genode::String<8> acpi_value_string;
				Genode::String<32> to_key;

				Genode::String<16> key_type("PRESS_RELEASE");
				Keys::Type press_release = Keys::Type::PRESS_RELEASE;

				map_node.attribute("acpi").value(&acpi_type);
				map_node.attribute("to_key").value(&to_key);
				try {
					map_node.attribute("as").value(&key_type);
					if (key_type == "PRESS")
						press_release = Keys::Type::PRESS;
					else if (key_type == "RELEASE")
						press_release = Keys::Type::RELEASE;
					else if (key_type != "PRESS_RELEASE")
						throw 0;
				} catch (Xml_node::Nonexistent_attribute) { }

				if (acpi_type == "lid" || acpi_type == "ac") {
					map_node.attribute("value").value(&acpi_value_string);

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
					map_node.attribute("value").value(&acpi_value);

				Input::Keycode key_code = Input::Keycode::KEY_UNKNOWN;

				if (to_key == "KEY_VENDOR")
					key_code = Input::Keycode::KEY_VENDOR;
				else if (to_key == "KEY_POWER")
					key_code = Input::Keycode::KEY_POWER;
				else if (to_key == "KEY_SLEEP")
					key_code = Input::Keycode::KEY_SLEEP;
				else if (to_key == "KEY_WAKEUP")
					key_code = Input::Keycode::KEY_WAKEUP;
				else if (to_key == "KEY_BATTERY")
					key_code = Input::Keycode::KEY_BATTERY;
				else if (to_key == "KEY_BRIGHTNESSUP")
					key_code = Input::Keycode::KEY_BRIGHTNESSUP;
				else if (to_key == "KEY_BRIGHTNESSDOWN")
					key_code = Input::Keycode::KEY_BRIGHTNESSDOWN;

				if (key_code == Input::Keycode::KEY_UNKNOWN)
					throw 4;

				if (acpi_type == "ec")
					Keys::insert_ec(new (_heap) Keys(key_code, acpi_value,
					                                 press_release));
				else if (acpi_type == "fixed")
					Keys::insert_special(new (_heap) Keys(key_code,
					                                      ACPI_POWER_BUTTON,
					                                      press_release));
				else if (acpi_type == "lid" || acpi_type == "ac")
					Keys::insert_special(new (_heap) Keys(key_code, acpi_value,
					                                      press_release));
				else if (acpi_type == "battery")
					Keys::insert_special(new (_heap) Keys(key_code,
					                                      ACPI_BATTERY,
					                                      press_release));
				else
					throw 5;
			} catch (...) {
				String<64> invalid_node(Genode::Cstring(map_node.addr(),
				                                        map_node.size()));
				Genode::error("map item : '", invalid_node.string(), "'");
				/* we want a well formated configuration ! - die */
				class Invalid_config {} exception;
				throw exception;
			}
		});

		_acpi_ac.sigh(_dispatch_acpi_ac);
		_acpi_battery.sigh(_dispatch_acpi_battery);
		_acpi_ec.sigh(_dispatch_acpi_ec);
		_acpi_fixed.sigh(_dispatch_acpi_fixed);
		_acpi_lid.sigh(_dispatch_acpi_lid);

		env.parent().announce(env.ep().manage(_root));

		/* check for initial valid ACPI data */
		check_acpi_ac();
		check_acpi_battery();
		check_acpi_ec();
		check_acpi_fixed();
		check_acpi_lid();
	}

	void check_acpi_ec()
	{
		_acpi_ec.update();

		if (!_acpi_ec.is_valid()) return;

		Xml_node ec_event(_acpi_ec.local_addr<char>(), _acpi_ec.size());

		ec_event.for_each_sub_node("ec", [&] (Xml_node ec_node) {
			ec_node.for_each_sub_node("data", [&] (Xml_node data_node) {
				try {
					long acpi_value = 0;
					unsigned long acpi_count = 0;

					data_node.attribute("value").value(&acpi_value);
					data_node.attribute("count").value(&acpi_count);

					Keys * key = Keys::find_by_ec(acpi_value);
					if (!key)
						return;

					if (!key->update_count(acpi_count))
						return;

					submit_input(key);
				} catch (...) { /* be robust - ignore ill-formated ACPI data */ }
			});
		});
	}

	void submit_input(Keys * key)
	{
		if (!key)
			return;

		if (key->type() == Keys::Type::PRESS_RELEASE ||
		    key->type() == Keys::Type::PRESS)
			_session.submit(Input::Event(Input::Event::PRESS, key->key_code(),
			                             0, 0, 0, 0));
		if (key->type() == Keys::Type::PRESS_RELEASE ||
		    key->type() == Keys::Type::RELEASE)
			_session.submit(Input::Event(Input::Event::RELEASE,
			                             key->key_code(), 0, 0, 0, 0));
	}

	void check_acpi_fixed()
	{
		_acpi_fixed.update();

		if (!_acpi_fixed.is_valid()) return;

		Xml_node fixed_event(_acpi_fixed.local_addr<char>(), _acpi_fixed.size());

		fixed_event.for_each_sub_node("power_button", [&] (Xml_node pw_node) {
			try {
				bool pressed = false;
				unsigned long acpi_count = 0;

				pw_node.attribute("value").value(&pressed);
				pw_node.attribute("count").value(&acpi_count);

				Keys * key = Keys::find_by_fixed(ACPI_POWER_BUTTON);
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

		if (!_acpi_battery.is_valid()) return;

		/* make use of it if we need to ... */
		Xml_node battery_node(_acpi_battery.local_addr<char>(),
		                      _acpi_battery.size());

		submit_input(Keys::find_by_fixed(ACPI_BATTERY));
	}

	void check_acpi_ac()
	{
		_acpi_ac.update();

		if (!_acpi_ac.is_valid()) return;

		Xml_node ac_node(_acpi_ac.local_addr<char>(), _acpi_ac.size());

		_check_acpi(ac_node, "ac", ACPI_AC_ONLINE, ACPI_AC_OFFLINE);
	}

	void check_acpi_lid()
	{
		_acpi_lid.update();

		if (!_acpi_lid.is_valid()) return;

		Xml_node lid_node(_acpi_lid.local_addr<char>(), _acpi_lid.size());

		_check_acpi(lid_node, "lid", ACPI_LID_OPEN, ACPI_LID_CLOSED);
	}

	void _check_acpi(Xml_node &xml_node, const char * sub_name,
	                 const unsigned state_o, const unsigned state_c)
	{
		xml_node.for_each_sub_node(sub_name, [&] (Xml_node node) {
			try {
				unsigned acpi_value = 0;
				unsigned long acpi_count = 0;

				node.attribute("value").value(&acpi_value);
				node.attribute("count").value(&acpi_count);

				enum { STATE_C = 0, STATE_O = 1 };

				Keys * key = nullptr;
				if (acpi_value == STATE_O)
					key = Keys::find_by_fixed(state_o);
				if (acpi_value == STATE_C)
					key = Keys::find_by_fixed(state_c);
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


void Component::construct(Genode::Env &env) { static Transform::Main main(env); }
