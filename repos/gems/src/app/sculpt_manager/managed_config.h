/*
 * \brief  Management of configurations that can be overridden by the user
 * \author Norman Feske
 * \date   2018-05-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MANAGED_CONFIG_H_
#define _MANAGED_CONFIG_H_

/* Genode includes */
#include <os/reporter.h>
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <types.h>

namespace Sculpt { template <typename> struct Managed_config; }


template <typename HANDLER>
struct Sculpt::Managed_config
{
	Env &_env;

	using Xml_node_name = String<20>;
	using Rom_name      = String<32>;
	using Label         = Session_label;

	enum Mode { MANAGED, MANUAL } _mode { MANAGED };

	HANDLER &_obj;

	void (HANDLER::*_handle) (Xml_node const &);

	/*
	 * Configuration supplied by the user
	 */
	Attached_rom_dataspace _manual_config_rom;

	/*
	 * Resulting configuration
	 */
	Expanding_reporter _config;

	Signal_handler<Managed_config> _manual_config_handler {
		_env.ep(), *this, &Managed_config::_handle_manual_config };

	/**
	 * Update manual config, decide between manual or managed mode of operation
	 */
	void _update_manual_config_rom()
	{
		_manual_config_rom.update();

		_mode = _manual_config_rom.xml().has_type("empty") ? MANAGED : MANUAL;
	}

	void _handle_manual_config()
	{
		_update_manual_config_rom();

		(_obj.*_handle)(_manual_config_rom.xml());
	}

	void with_manual_config(auto const &fn) const
	{
		fn(_manual_config_rom.xml());
	}

	/**
	 * \return true if manually-managed configuration could be used
	 */
	bool try_generate_manually_managed()
	{
		if (_mode == MANAGED)
			return false;

		/*
		 * If a manually managed config at 'config/' is provided, copy its
		 * content to the effective config at 'config/managed/'.
		 */
		_config.generate(_manual_config_rom.xml());
		return true;
	}

	void generate(auto const &fn)
	{
		_config.generate([&] (Xml_generator &xml) { fn(xml); });
	}

	Managed_config(Env &env, Xml_node_name const &xml_node_name,
	               Rom_name const &rom_name,
	               HANDLER &obj, void (HANDLER::*handle) (Xml_node const &))
	:
		_env(env), _obj(obj), _handle(handle),
		_manual_config_rom(_env, Label("config -> ", rom_name).string()),
		_config(_env, xml_node_name.string(), Label(rom_name, "_config").string())
	{
		_manual_config_rom.sigh(_manual_config_handler);

		/* determine initial '_mode' */
		_update_manual_config_rom();
	}

	void trigger_update() { _manual_config_handler.local_submit(); }
};

#endif /* _MANAGED_CONFIG_H_ */
