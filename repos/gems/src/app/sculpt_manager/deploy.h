/*
 * \brief  Sculpt deploy management
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DEPLOY_H_
#define _DEPLOY_H_

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* included from depot_deploy tool */
#include <children.h>

/* local includes */
#include <types.h>
#include <runtime.h>
#include <managed_config.h>

namespace Sculpt { struct Deploy; }


struct Sculpt::Deploy
{
	Env &_env;

	Allocator &_alloc;

	Runtime_config_generator &_runtime_config_generator;

	typedef String<16> Arch;
	Arch _arch { };

	struct Query_version { unsigned value; } _query_version { 0 };

	struct Depot_rom_state { Ram_quota ram_quota; };

	Depot_rom_state depot_rom_state { 32*1024*1024 };

	Attached_rom_dataspace _manual_deploy_rom { _env, "config -> deploy" };

	Attached_rom_dataspace _blueprint_rom { _env, "report -> runtime/depot_query/blueprint" };

	Expanding_reporter _depot_query_reporter { _env, "query", "depot_query"};

	bool _manual_installation_scheduled = false;

	Managed_config<Deploy> _installation {
		_env, "installation", "installation", *this, &Deploy::_handle_installation };

	void _handle_installation(Xml_node manual_config)
	{
		_manual_installation_scheduled = manual_config.has_sub_node("archive");
		handle_deploy();
	}

	Depot_deploy::Children _children { _alloc };

	bool update_needed() const
	{
		return _manual_installation_scheduled || _children.any_incomplete();
	}

	void handle_deploy();

	void _handle_manual_deploy()
	{
		_manual_deploy_rom.update();
		_query_version.value++;
		handle_deploy();
	}

	void _handle_blueprint()
	{
		_blueprint_rom.update();
		handle_deploy();
	}

	void gen_runtime_start_nodes(Xml_generator &) const;

	Signal_handler<Deploy> _manual_deploy_handler {
		_env.ep(), *this, &Deploy::_handle_manual_deploy };

	Signal_handler<Deploy> _blueprint_handler {
		_env.ep(), *this, &Deploy::_handle_blueprint };

	void restart()
	{
		/* ignore stale query results */
		_query_version.value++;

		_children.apply_config(Xml_node("<config/>"));
	}

	void reattempt_after_installation()
	{
		_children.reset_incomplete();
		handle_deploy();
	}

	Deploy(Env &env, Allocator &alloc,
	       Runtime_config_generator &runtime_config_generator)
	:
		_env(env), _alloc(alloc),
		_runtime_config_generator(runtime_config_generator)
	{
		_manual_deploy_rom.sigh(_manual_deploy_handler);
		_blueprint_rom.sigh(_blueprint_handler);
	}
};

#endif /* _DEPLOY_H_ */
