/*
 * \brief  Representation of a session request
 * \author Norman Feske
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/session_state.h>
#include <base/service.h>
#include <util/arg_string.h>

using namespace Genode;


void Session_state::print(Output &out) const
{
	using Genode::print;

	print(out, "service=", _service.name(), " cid=", _id_at_client, " "
	      "args='", _args, "' state=", (int)phase, " "
	      "ram_quota=", _donated_ram_quota);
}


void Session_state::generate_session_request(Xml_generator &xml) const
{
	if (!id_at_server.constructed())
		warning(__func__, ": id_at_server not initialized");

	switch (phase) {

	case CREATE_REQUESTED:

		xml.node("create", [&] () {
			xml.attribute("id", id_at_server->id().value);
			xml.attribute("service", _service.name());
			xml.node("args", [&] () {
				xml.append_sanitized(_args.string());
			});
		});
		break;

	case UPGRADE_REQUESTED:

		xml.node("upgrade", [&] () {
			xml.attribute("id", id_at_server->id().value);
			xml.attribute("ram_quota", ram_upgrade);
		});
		break;

	case CLOSE_REQUESTED:

		xml.node("close", [&] () {
			xml.attribute("id", id_at_server->id().value); });
		break;

	case INVALID_ARGS:
	case AVAILABLE:
	case CAP_HANDED_OUT:
	case CLOSED:
		break;
	}
}


void Session_state::destroy()
{
	/*
	 * Manually release client-side ID so that static env sessions are
	 * immediately by removed from the cliend ID space when 'destroy' is
	 * called. Otherwise, the iterative cleanup of the cliend ID space
	 * via 'apply_any' would end up in an infinite loop.
	 */
	_id_at_client.destruct();

	if (_factory)
		_factory->_destroy(*this);
}


Session_state::Session_state(Service                  &service,
                             Id_space<Parent::Client> &client_id_space,
                             Parent::Client::Id        client_id,
                             Args const               &args,
                             Affinity           const &affinity)
:
	_service(service),
	_donated_ram_quota(Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0)),
	_id_at_client(*this, client_id_space, client_id),
	_args(args), _affinity(affinity)
{ }
