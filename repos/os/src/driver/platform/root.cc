/*
 * \brief  Platform driver - root component
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

using Version = Driver::Session_component::Policy_version;

void Driver::Root::update_policy()
{
	_sessions.for_each([&] (Session_component &sc)
	{
		with_matching_policy(sc._label, _config.node(),
			[&] (Node const &policy) {
				sc.update_policy(policy.attribute_value("info", false),
				                 policy.attribute_value("version", Version())); },
			[&] {
				error("No matching policy for '", sc._label.string(),
				      "' anymore, will close the session!");
				close(sc.cap());
			});
	});
}


Driver::Root::Create_result Driver::Root::_create_session(const char *args)
{
	return with_matching_policy(label_from_args(args), _config.node(),

		[&] (Node const &policy) {
			return _alloc_obj(_env, _config, _devices, _sessions,
			                  label_from_args(args),
			                  session_resources_from_args(args),
			                  policy.attribute_value("info", false),
			                  policy.attribute_value("version", Version()));
		},
		[&] () -> Create_result {
			error("Invalid session request, no matching policy for ",
			      "'", label_from_args(args), "'");
			return Create_error::DENIED;
		});
}


void Driver::Root::_upgrade_session(Session_component &sc, const char * args)
{
	sc.upgrade(ram_quota_from_args(args));
	sc.upgrade(cap_quota_from_args(args));
}


Driver::Root::Root(Env                          &env,
                   Sliced_heap                  &sliced_heap,
                   Attached_rom_dataspace const &config,
                   Device_model                 &devices)
:
	Root_component<Session_component>(env.ep(), sliced_heap),
	_env(env), _config(config), _devices(devices)
{ }
