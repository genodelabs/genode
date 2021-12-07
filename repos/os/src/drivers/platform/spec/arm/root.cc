/*
 * \brief  Platform driver for ARM root component
 * \author Stefan Kalkowski
 * \date   2020-04-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <root.h>

void Driver::Root::update_policy()
{
	_sessions.for_each([&] (Session_component & sc)
	{
		try {
			Session_policy const policy { sc._label, _env.config.xml() };
			sc.update_policy(policy.attribute_value("info", false));
		}
		catch (Session_policy::No_policy_defined) {
			error("No matching policy for '", sc._label.string(),
			      "' anymore, will close the session!");
			close(sc.cap());
		}
	});
}


Driver::Session_component * Driver::Root::_create_session(const char *args)
{
	Session_component * sc = nullptr;

	try {
		Session::Label const label  { session_label_from_args(args) };
		Session_policy const policy { label, _env.config.xml()      };

		sc = new (md_alloc())
			Session_component(_env, _sessions, label,
			                  session_resources_from_args(args),
			                  session_diag_from_args(args),
			                  policy.attribute_value("info", false));
	} catch (Session_policy::No_policy_defined) {
		error("Invalid session request, no matching policy for ",
		      "'", label_from_args(args).string(), "'");
		throw Service_denied();
	} catch (...) {
		if (sc) { Genode::destroy(md_alloc(), sc); }
		throw;
	}

	return sc;
}


void Driver::Root::_upgrade_session(Session_component * sc, const char * args)
{
	sc->upgrade(ram_quota_from_args(args));
	sc->upgrade(cap_quota_from_args(args));
}


Driver::Root::Root(Driver::Env & env)
: Root_component<Session_component>(env.env.ep(), env.sliced_heap),
  _env(env) { }
