/*
 * \brief  Configuration of keyboard mode indicators
 * \author Norman Feske
 * \date   2017-10-25
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__LED_STATE_H_
#define _DRIVERS__INPUT__SPEC__LED_STATE_H_

#include <util/xml_node.h>
#include <util/reconstructible.h>
#include <base/component.h>

namespace Ps2 { struct Led_state; }


struct Ps2::Led_state
{
	Genode::Env &_env;

	typedef Genode::String<32> Name;

	Name const _name;

	Genode::Constructible<Genode::Attached_rom_dataspace> _rom { };

	bool _enabled = false;

	Led_state(Genode::Env &env, Name const &name) : _env(env), _name(name) { }

	void update(Genode::Xml_node config, Genode::Signal_context_capability sigh)
	{
		typedef Genode::String<32> Attr;
		typedef Genode::String<16> Value;

		Attr  const attr(_name, "_led");
		Value const value = config.attribute_value(attr.string(), Value());

		bool const rom_configured = (value == "rom");

		if (rom_configured && !_rom.constructed()) {
			_rom.construct(_env, _name.string());
			_rom->sigh(sigh);
		}

		if (!rom_configured && _rom.constructed())
			_rom.destruct();

		if (_rom.constructed())
			_rom->update();

		_enabled = _rom.constructed() ? _rom->xml().attribute_value("enabled", false)
		                              : config.attribute_value(attr.string(), false);
	}

	bool enabled() const { return _enabled; }
};

#endif /* _DRIVERS__INPUT__SPEC__LED_STATE_H_ */
