/*
 * \brief  Representation of a session request
 * \author Norman Feske
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/session_state.h>
#include <base/service.h>
#include <util/arg_string.h>

using namespace Genode;


struct Formatted_phase
{
	Session_state::Phase _phase;

	Formatted_phase(Session_state::Phase phase) : _phase(phase) { }

	void print(Output &output) const
	{
		using Genode::print;
		typedef Genode::Session_state State;

		switch (_phase) {
		case State::CREATE_REQUESTED:   print(output, "CREATE_REQUESTED");  break;
		case State::INVALID_ARGS:       print(output, "INVALID_ARGS");      break;
		case State::QUOTA_EXCEEDED:     print(output, "QUOTA_EXCEEDED");    break;
		case State::AVAILABLE:          print(output, "AVAILABLE");         break;
		case State::CAP_HANDED_OUT:     print(output, "CAP_HANDED_OUT");    break;
		case State::UPGRADE_REQUESTED:  print(output, "UPGRADE_REQUESTED"); break;
		case State::CLOSE_REQUESTED:    print(output, "CLOSE_REQUESTED");   break;
		case State::CLOSED:             print(output, "CLOSED");            break;
		}
	}
};


void Session_state::print(Output &out) const
{
	using Genode::print;

	print(out, "service=", _service.name(), " cid=", _id_at_client, " "
	      "args='", _args, "' state=", Formatted_phase(phase), " "
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
			xml.attribute("label", _label);
			xml.node("args", [&] () {
				xml.append_sanitized(Server_args(*this).string());
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
	case QUOTA_EXCEEDED:
	case AVAILABLE:
	case CAP_HANDED_OUT:
	case CLOSED:
		break;
	}
}


void Session_state::generate_client_side_info(Xml_generator &xml, Detail detail) const
{
	xml.attribute("service", _service.name());
	xml.attribute("label", _label);
	xml.attribute("state", String<32>(Formatted_phase(phase)));
	xml.attribute("ram", String<32>(Number_of_bytes(_donated_ram_quota)));

	if (detail.args == Detail::ARGS)
		xml.node("args", [&] () { xml.append_sanitized(_args.string()); });
}


void Session_state::generate_server_side_info(Xml_generator &xml, Detail detail) const
{
	generate_client_side_info(xml, detail);
}


void Session_state::destroy()
{
	/*
	 * Manually release client-side ID so that static env sessions are
	 * immediately removed from the client ID space when 'destroy' is
	 * called. Otherwise, the iterative cleanup of the cliend ID space
	 * via 'apply_any' would end up in an infinite loop.
	 */
	_id_at_client.destruct();

	/*
	 * Manually release the server-side ID of the session from server ID space
	 * to make sure that the iterative cleanup of child-provided sessions (in
	 * the 'Child' destructor) always terminates regardless of the used
	 * session-state factory.
	 *
	 * In particular, if the to-be-destructed child provided an environment
	 * session of another child, there is no factory for such an environment
	 * session. The server-ID destructor of such an environment session would
	 * be called not before destructing the corresponent 'Env_connection' of
	 * the client, independent from the destruction of the session-providing
	 * child.
	 */
	id_at_server.destruct();

	/*
	 * Make sure that the session does not appear as being alive. I.e., if
	 * 'destroy' was called during the destruction of a service, prevent the
	 * 'Local_connection' destructor of a dangling session to initiate a close
	 * request to the no-longer existing service.
	 */
	phase = CLOSED;

	if (_factory)
		_factory->_destroy(*this);
}


Session_state::Session_state(Service                  &service,
                             Id_space<Parent::Client> &client_id_space,
                             Parent::Client::Id        client_id,
                             Session_label      const &label,
                             Args const               &args,
                             Affinity           const &affinity)
:
	_service(service),
	_donated_ram_quota(Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0)),
	_id_at_client(*this, client_id_space, client_id),
	_label(label), _args(args), _affinity(affinity)
{ }
