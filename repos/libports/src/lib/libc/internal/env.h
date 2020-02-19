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


class Libc::Env_implementation : public Libc::Env, public Config_accessor
{
	private:

		Genode::Env &_env;

		Attached_rom_dataspace _config { _env, "config" };

		Xml_node _vfs_config()
		{
			try { return _config.xml().sub_node("vfs"); }
			catch (Xml_node::Nonexistent_sub_node) { }
			try {
				Xml_node node = _config.xml().sub_node("libc").sub_node("vfs");
				warning("'<config> <libc> <vfs/>' is deprecated, "
				        "please move to '<config> <vfs/>'");
				return node;
			}
			catch (Xml_node::Nonexistent_sub_node) { }

			return Xml_node("<vfs/>");
		}

		Xml_node _libc_config()
		{
			try { return _config.xml().sub_node("libc"); }
			catch (Xml_node::Nonexistent_sub_node) { }

			return Xml_node("<libc/>");
		}

		Vfs::Simple_env _vfs_env;

		Xml_node _config_xml() const override {
			return _config.xml(); };

	public:

		Env_implementation(Genode::Env &env, Genode::Allocator &alloc)
		: _env(env), _vfs_env(_env, alloc, _vfs_config()) { }


		Vfs::Env &vfs_env() { return _vfs_env; }


		/*************************
		 ** Libc::Env interface **
		 *************************/

		Vfs::File_system &vfs() override {
			return _vfs_env.root_dir(); }

		Xml_node libc_config() override {
			return _libc_config(); }


		/*************************************
		 ** Libc::Config_accessor interface **
		 *************************************/

		Xml_node config() const override { return _config.xml(); }


		/***************************
		 ** Genode::Env interface **
		 ***************************/

		Parent        &parent() override { return _env.parent(); }
		Cpu_session   &cpu()    override { return _env.cpu(); }
		Region_map    &rm()     override { return _env.rm(); }
		Pd_session    &pd()     override { return _env.pd(); }
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

		void upgrade(Parent::Client::Id id,
		             Parent::Upgrade_args const &args) override {
			return _env.upgrade(id, args); }

		void close(Parent::Client::Id id) override {
			return _env.close(id); }

		/* already done by the libc */
		void exec_static_constructors() override { }

		void reinit(Native_capability::Raw raw) override {
			_env.reinit(raw); }

		void reinit_main_thread(Capability<Region_map> &stack_area_rm) override {
			_env.reinit_main_thread(stack_area_rm); }
};

#endif /* _LIBC__INTERNAL__ENV_H_ */
