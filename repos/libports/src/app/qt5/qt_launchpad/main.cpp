/*
 * \brief   Qt Launchpad main program
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/* local includes */
#include "qt_launchpad.h"

/* Qt includes */
#include <QtGui>
#include <QApplication>

/* Genode includes */
#include <base/component.h>
#include <base/env.h>

extern int genode_argc;
extern char **genode_argv;

namespace Qt_launchpad_namespace {
	struct Local_env;
	using namespace Genode;
}

struct Qt_launchpad_namespace::Local_env : Genode::Env
{
	Genode::Env        &genode_env;

	Genode::Entrypoint  local_ep { genode_env,
	                               2*1024*sizeof(addr_t),
	                               "qt_launchpad_ep" };

	Local_env(Env &genode_env) : genode_env(genode_env) { }

	Parent &parent()                         { return genode_env.parent(); }
	Ram_session &ram()                       { return genode_env.ram(); }
	Cpu_session &cpu()                       { return genode_env.cpu(); }
	Region_map &rm()                         { return genode_env.rm(); }
	Pd_session &pd()                         { return genode_env.pd(); }
	Entrypoint &ep()                         { return local_ep; }
	Ram_session_capability ram_session_cap() { return genode_env.ram_session_cap(); }
	Cpu_session_capability cpu_session_cap() { return genode_env.cpu_session_cap(); }
	Pd_session_capability pd_session_cap()   { return genode_env.pd_session_cap(); }
	Id_space<Parent::Client> &id_space()     { return genode_env.id_space(); }

	Session_capability session(Parent::Service_name const &service_name,
	                           Parent::Client::Id id,
	                           Parent::Session_args const &session_args,
	                           Affinity             const &affinity)
	{ return genode_env.session(service_name, id, session_args, affinity); }

	void upgrade(Parent::Client::Id id, Parent::Upgrade_args const &args)
	{ return genode_env.upgrade(id, args); }

	void close(Parent::Client::Id id) { return genode_env.close(id); }
};

void Component::construct(Genode::Env &env)
{
	static Qt_launchpad_namespace::Local_env local_env(env);

	static QApplication a(genode_argc, genode_argv);

	static Qt_launchpad launchpad(local_env, env.ram().avail());

	try {
		launchpad.process_config();
	} catch (...) { }

	launchpad.move(300,100);
	launchpad.show();

	a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

	a.exec();
}
