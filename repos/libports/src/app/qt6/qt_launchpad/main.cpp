/*
 * \brief   Qt Launchpad main program
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/*
 * Copyright (C) 2008-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>

/* Qt includes */
#include <QtGui>
#include <QApplication>

/* qt6_component includes */
#include <qt6_component/qpa_init.h>

/* local includes */
#include "qt_launchpad.h"

namespace Qt_launchpad_namespace {
	struct Local_env;
	using namespace Genode;
}

struct Qt_launchpad_namespace::Local_env : Genode::Env
{
	Genode::Env        &genode_env;

	Genode::Entrypoint  local_ep { genode_env,
	                               2*1024*sizeof(addr_t),
	                               "qt_launchpad_ep",
	                               Affinity::Location() };

	Local_env(Env &genode_env) : genode_env(genode_env) { }

	Parent &parent()                         override { return genode_env.parent(); }
	Cpu_session &cpu()                       override { return genode_env.cpu(); }
	Region_map &rm()                         override { return genode_env.rm(); }
	Pd_session &pd()                         override { return genode_env.pd(); }
	Entrypoint &ep()                         override { return local_ep; }
	Cpu_session_capability cpu_session_cap() override { return genode_env.cpu_session_cap(); }
	Pd_session_capability pd_session_cap()   override { return genode_env.pd_session_cap(); }
	Id_space<Parent::Client> &id_space()     override { return genode_env.id_space(); }

	Session_capability session(Parent::Service_name const &service_name,
	                           Parent::Client::Id id,
	                           Parent::Session_args const &session_args,
	                           Affinity             const &affinity) override
	{
		return genode_env.session(service_name, id, session_args, affinity);
	}

	Session_capability try_session(Parent::Service_name const &service_name,
	                               Parent::Client::Id id,
	                               Parent::Session_args const &session_args,
	                               Affinity             const &affinity) override
	{
		return genode_env.try_session(service_name, id, session_args, affinity);
	}

	void upgrade(Parent::Client::Id id, Parent::Upgrade_args const &args) override
	{
		return genode_env.upgrade(id, args);
	}

	void close(Parent::Client::Id id) override { return genode_env.close(id); }

	void exec_static_constructors() override { }
};


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		qpa_init(env);

		Qt_launchpad_namespace::Local_env local_env(env);

		int argc = 1;
		char const *argv[] = { "qt_launchpad", 0 };

		QApplication a(argc, (char**)argv);

		Qt_launchpad launchpad(local_env, env.pd().avail_ram().value);

		Genode::Attached_rom_dataspace config(env, "config");

		try { launchpad.process_config(config.xml()); } catch (...) { }

		launchpad.move(300,100);
		launchpad.show();

		a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

		a.exec();
	});
}
