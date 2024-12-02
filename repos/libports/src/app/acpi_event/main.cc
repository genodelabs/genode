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

		static auto with_key(Avl_tree<Keys> &map, uint64_t acpi_code, auto const &fn)
		{
			if (!map.first())
				return;

			Keys * const key_ptr = map.first()->_find_by_acpi_value(acpi_code);
			if (key_ptr)
				fn(*key_ptr);
		}
};


struct Transform::Main
{
	Env &_env;

	enum {
		ACPI_POWER_BUTTON, ACPI_LID_OPEN, ACPI_LID_CLOSED,
		ACPI_AC_ONLINE, ACPI_AC_OFFLINE, ACPI_BATTERY
	};

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Attached_rom_dataspace
		_acpi_ac      { _env, "acpi_ac"      },
		_acpi_battery { _env, "acpi_battery" },
		_acpi_ec      { _env, "acpi_ec"      },
		_acpi_fixed   { _env, "acpi_fixed"   },
		_acpi_lid     { _env, "acpi_lid"     },
		_acpi_hid     { _env, "acpi_hid"     };

	Signal_handler<Main>
		_acpi_ac_handler      { _env.ep(), *this, &Main::_handle_acpi_ac      },
		_acpi_battery_handler { _env.ep(), *this, &Main::_handle_acpi_battery },
		_acpi_ec_handler      { _env.ep(), *this, &Main::_handle_acpi_ec      },
		_acpi_fixed_handler   { _env.ep(), *this, &Main::_handle_acpi_fixed   },
		_acpi_lid_handler     { _env.ep(), *this, &Main::_handle_acpi_lid     },
		_acpi_hid_handler     { _env.ep(), *this, &Main::_handle_acpi_hid     };

	Event::Connection _event { _env };

	Avl_tree<Keys> _map_ec      { };
	Avl_tree<Keys> _map_hid     { };
	Avl_tree<Keys> _map_special { };

	Main(Env &env) : _env(env)
	{
		_config.xml().for_each_sub_node("map", [&] (Xml_node const &map_node) {

			auto const acpi_type    = map_node.attribute_value("acpi",   String<8>());
			auto const to_key       = map_node.attribute_value("to_key", Input::Key_name());
			auto const value_string = map_node.attribute_value("value",  String<8>());
			auto const key_type     = map_node.attribute_value("as",     String<16>("PRESS_RELEASE"));

			auto press_release = [&] () -> Keys::Type
			{
				if (key_type == "PRESS")         return Keys::Type::PRESS;
				if (key_type == "RELEASE")       return Keys::Type::RELEASE;
				if (key_type == "PRESS_RELEASE") return Keys::Type::PRESS_RELEASE;
				warning("unsupported 'as' attribute value \"", key_type, "\"");
				return Keys::Type::PRESS_RELEASE;
			};

			auto acpi_value = [&] () -> uint64_t
			{
				if (acpi_type == "lid") {
					if (value_string == "OPEN")   return ACPI_LID_OPEN;
					if (value_string == "CLOSED") return ACPI_LID_CLOSED;
					warning("unsupported lid value \"", value_string, "\"");
				}
				if (acpi_type == "ac") {
					if (value_string == "ONLINE")  return ACPI_AC_ONLINE;
					if (value_string == "OFFLINE") return ACPI_AC_OFFLINE;
					warning("unsupported ac value \"", value_string, "\"");
				}
				return map_node.attribute_value("value", uint64_t(0));
			};

			Input::Keycode const key_code = Input::key_code(to_key);
			if (key_code == Input::Keycode::KEY_UNKNOWN) {
				warning("unsupported 'to_key' attribute value \"", to_key, "\"");
				return;
			}

			if (acpi_type == "ec")
				_map_ec.insert(new (_heap) Keys(key_code, acpi_value(),
				                                press_release()));
			else if (acpi_type == "fixed")
				_map_special.insert(new (_heap) Keys(key_code,
				                                     ACPI_POWER_BUTTON,
				                                     press_release()));
			else if (acpi_type == "lid" || acpi_type == "ac")
				_map_special.insert(new (_heap) Keys(key_code, acpi_value(),
				                                     press_release()));
			else if (acpi_type == "battery")
				_map_special.insert(new (_heap) Keys(key_code,
				                                     ACPI_BATTERY,
				                                     press_release()));
			else if (acpi_type == "hid")
				_map_hid.insert(new (_heap) Keys(key_code, acpi_value(),
				                                 press_release()));
			else
				warning("unsupported 'acpi' attribute value \"", acpi_type, "\"");
		});

		_acpi_ac     .sigh(_acpi_ac_handler);
		_acpi_battery.sigh(_acpi_battery_handler);
		_acpi_ec     .sigh(_acpi_ec_handler);
		_acpi_fixed  .sigh(_acpi_fixed_handler);
		_acpi_lid    .sigh(_acpi_lid_handler);
		_acpi_hid    .sigh(_acpi_hid_handler);

		/* check for initial valid ACPI data */
		_handle_acpi_ac();
		_handle_acpi_battery();
		_handle_acpi_ec();
		_handle_acpi_fixed();
		_handle_acpi_lid();
		_handle_acpi_hid();
	}

	void submit_input(Keys &key)
	{
		_event.with_batch([&] (Event::Session_client::Batch &batch) {

			if (key.type() == Keys::Type::PRESS_RELEASE ||
			    key.type() == Keys::Type::PRESS)
				batch.submit(Input::Press{key.key_code()});

			if (key.type() == Keys::Type::PRESS_RELEASE ||
			    key.type() == Keys::Type::RELEASE)
				batch.submit(Input::Release{key.key_code()});
		});
	}

	void _check_acpi(Attached_rom_dataspace &rom, Avl_tree<Keys> &map,
	                 char const * const name)
	{
		rom.update();
		rom.xml().for_each_sub_node(name, [&] (Xml_node const &node) {
			node.for_each_sub_node("data", [&] (Xml_node const &data_node) {
				uint64_t const value = data_node.attribute_value("value", 0ULL);
				uint64_t const count = data_node.attribute_value("count", 0ULL);

				Keys::with_key(map, value, [&] (Keys &key) {
					if (key.update_count(count))
						submit_input(key); });
			});
		});
	}

	void _check_acpi(Xml_node const &xml_node, const char * const sub_name,
	                 unsigned const state_o, unsigned const state_c)
	{
		xml_node.for_each_sub_node(sub_name, [&] (Xml_node const &node) {
			uint64_t const value = node.attribute_value("value", 0ULL);
			uint64_t const count = node.attribute_value("count", 0ULL);

			uint64_t const STATE_C = 0, STATE_O = 1;

			if (value != STATE_O && value != STATE_C)
				return;

			unsigned const state = (value == STATE_O) ? state_o : state_c;

			Keys::with_key(_map_special, state, [&] (Keys &key) {
				if (key.update_count(count))
					submit_input(key); });
		});
	}

	void _handle_acpi_ec() { _check_acpi(_acpi_ec, _map_ec, "ec"); }

	void _handle_acpi_hid() { _check_acpi(_acpi_hid, _map_hid, "hid"); }

	void _handle_acpi_fixed()
	{
		_acpi_fixed.update();
		Keys::with_key(_map_special, ACPI_POWER_BUTTON, [&] (Keys &key) {
			_acpi_fixed.xml().for_each_sub_node("power_button", [&] (Xml_node const &pw) {
				bool     const pressed = pw.attribute_value("value", false);
				uint64_t const count   = pw.attribute_value("count", 0UL);

				if (key.update_count(count) && pressed)
					submit_input(key);
			});
		});
	}

	void _handle_acpi_battery()
	{
		_acpi_battery.update();
		Keys::with_key(_map_special, ACPI_BATTERY, [&] (Keys &key) {
			submit_input(key); });
	}

	void _handle_acpi_ac()
	{
		_acpi_ac.update();
		_check_acpi(_acpi_ac.xml(), "ac", ACPI_AC_ONLINE, ACPI_AC_OFFLINE);
	}

	void _handle_acpi_lid()
	{
		_acpi_lid.update();
		_check_acpi(_acpi_lid.xml(), "lid", ACPI_LID_OPEN, ACPI_LID_CLOSED);
	}
};


void Component::construct(Genode::Env &env) {
	static Transform::Main main(env); }
