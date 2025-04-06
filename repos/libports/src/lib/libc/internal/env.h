/*
 * \brief  Libc environment
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__ENV_H_
#define _LIBC__INTERNAL__ENV_H_

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <vfs/simple_env.h>

/* libc includes */
#include <libc/component.h>  /* 'Libc::Env' */

namespace Libc { class Env_implementation; }


class Libc::Env_implementation : public Libc::Env
{
	private:

		Genode::Env &_env;

		Vfs::Env &_vfs_env;

		Attached_rom_dataspace const &_config_rom;

	public:

		Env_implementation(Genode::Env &env, Vfs::Env &vfs_env,
		                   Attached_rom_dataspace const &config_rom)
		:
			_env(env), _vfs_env(vfs_env), _config_rom(config_rom)
		{ }

		Vfs::File_system &vfs() { return _vfs_env.root_dir(); }


		/*************************
		 ** Libc::Env interface **
		 *************************/

		void _with_config(With_config::Ft const &fn) const override
		{
			fn(_config_rom.xml());
		}

		Vfs::Env &vfs_env() override { return _vfs_env; }


		/***************************
		 ** Genode::Env interface **
		 ***************************/

		Parent        &parent() override { return _env.parent(); }
		Cpu_session   &cpu()    override { return _env.cpu(); }
		Env::Local_rm &rm()     override { return _env.rm(); }
		Pd_session    &pd()     override { return _env.pd(); }
		Ram_allocator &ram()    override { return _env.ram(); }
		Entrypoint    &ep()     override { return _env.ep(); }

		Cpu_session_capability cpu_session_cap() override {
			return _env.cpu_session_cap(); }

		Pd_session_capability pd_session_cap() override {
			return _env.pd_session_cap(); }

		Id_space<Parent::Client> &id_space() override {
			return _env.id_space(); }

		Session_capability session(Parent::Service_name const &name,
		                           Parent::Client::Id id,
		                           Parent::Session_args const &args,
		                           Affinity             const &aff) override {
			return _env.session(name, id, args, aff); }

		Session_capability try_session(Parent::Service_name const &name,
		                               Parent::Client::Id id,
		                               Parent::Session_args const &args,
		                               Affinity             const &aff) override {
			return _env.try_session(name, id, args, aff); }

		void upgrade(Parent::Client::Id id,
		             Parent::Upgrade_args const &args) override {
			return _env.upgrade(id, args); }

		void close(Parent::Client::Id id) override {
			return _env.close(id); }

		/* already done by the libc */
		void exec_static_constructors() override { }
};

#endif /* _LIBC__INTERNAL__ENV_H_ */
