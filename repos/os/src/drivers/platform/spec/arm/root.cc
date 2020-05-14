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
	_sessions.for_each([&] (Session_component & sc) {

		bool     policy_changed = false;
		unsigned device_count   = 0;

		try {
			Genode::Session_policy const policy { sc._label, _config.xml() };

			policy.for_each_sub_node("device", [&] (Genode::Xml_node node) {
				device_count++;
				if (!sc.has_device(node.attribute_value("name",
				                                        Device::Name()))) {
					policy_changed = true; }
			});

			if (device_count != sc.devices_count()) { policy_changed = true; }
		}
		catch (Genode::Session_policy::No_policy_defined) {
			policy_changed = true;
			Genode::error("No matching policy for '", sc._label.string(),
			              "' anymore, will close the session!");
		}

		if (policy_changed) { close(sc.cap()); }
	});
}


Driver::Session_component * Driver::Root::_create_session(const char *args)
{
	using namespace Genode;

	Session_component * sc = nullptr;

	try {
		sc = new (md_alloc()) Session_component(_env,
		                                        _devices,
		                                        _sessions,
		                                        session_label_from_args(args),
		                                        session_resources_from_args(args),
		                                        session_diag_from_args(args));

		Session_policy const policy { sc->_label, _config.xml() };
		policy.for_each_sub_node("device", [&] (Xml_node node) {
			sc->add(node.attribute_value("name", Driver::Device::Name())); });
	} catch (Session_policy::No_policy_defined) {
		if (sc) { Genode::destroy(md_alloc(), sc); }
		error("Invalid session request, no matching policy for ",
		      "'", Genode::label_from_args(args).string(), "'");
		throw Service_denied();
	} catch (...) {
		if (sc) { Genode::destroy(md_alloc(), sc); }
		throw;
	}

	return sc;
}


void Driver::Root::_upgrade_session(Session_component * sc, const char * args)
{
	sc->upgrade(Genode::ram_quota_from_args(args));
	sc->upgrade(Genode::cap_quota_from_args(args));
}


Driver::Root::Root(Genode::Env                    & env,
                   Genode::Allocator              & alloc,
                   Genode::Attached_rom_dataspace & config,
                   Driver::Device_model           & devices)
: Genode::Root_component<Session_component>(env.ep(), alloc),
  _env(env), _config(config), _devices(devices) { }
