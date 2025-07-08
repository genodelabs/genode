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
		using State = Genode::Session_state;

		switch (_phase) {
		case State::CREATE_REQUESTED:       print(output, "CREATE_REQUESTED");       break;
		case State::SERVICE_DENIED:         print(output, "SERVICE_DENIED");         break;
		case State::INSUFFICIENT_RAM_QUOTA: print(output, "INSUFFICIENT_RAM_QUOTA"); break;
		case State::INSUFFICIENT_CAP_QUOTA: print(output, "INSUFFICIENT_CAP_QUOTA"); break;
		case State::AVAILABLE:              print(output, "AVAILABLE");              break;
		case State::CAP_HANDED_OUT:         print(output, "CAP_HANDED_OUT");         break;
		case State::UPGRADE_REQUESTED:      print(output, "UPGRADE_REQUESTED");      break;
		case State::CLOSE_REQUESTED:        print(output, "CLOSE_REQUESTED");        break;
		case State::CLOSED:                 print(output, "CLOSED");                 break;
		}
	}
};


void Session_state::print(Output &out) const
{
	using Genode::print;

	print(out, "service=", _service.name(), " cid=", _id_at_client, " "
	      "args='", _args, "' state=", Formatted_phase(phase), " "
	      "ram_quota=", _donated_ram_quota, ", cap_quota=", _donated_cap_quota);
}


void Session_state::generate_session_request(Generator &g) const
{
	if (!id_at_server.constructed())
		warning(__func__, ": id_at_server not initialized");

	switch (phase) {

	case CREATE_REQUESTED:

		g.node("create", [&] {
			g.attribute("id", id_at_server->id().value);
			g.attribute("service", _service.name());
			g.attribute("label", _label);
			g.node("args", [&] {
				g.append_quoted(Server_args(*this).string());
			});
			g.node("affinity", [&] {
				g.node("space", [&] {
					g.attribute("width",  _affinity.space().width());
					g.attribute("height", _affinity.space().height());
				});
				g.node("location", [&] {
					g.attribute("xpos",   _affinity.location().xpos());
					g.attribute("ypos",   _affinity.location().ypos());
					g.attribute("width",  _affinity.location().width());
					g.attribute("height", _affinity.location().height());
				});
			});
		});
		break;

	case UPGRADE_REQUESTED:

		g.node("upgrade", [&] {
			g.attribute("id", id_at_server->id().value);
			g.attribute("ram_quota", ram_upgrade.value);
			g.attribute("cap_quota", cap_upgrade.value);
		});
		break;

	case CLOSE_REQUESTED:

		g.node("close", [&] {
			g.attribute("id", id_at_server->id().value); });
		break;

	case SERVICE_DENIED:
	case INSUFFICIENT_RAM_QUOTA:
	case INSUFFICIENT_CAP_QUOTA:
	case AVAILABLE:
	case CAP_HANDED_OUT:
	case CLOSED:
		break;
	}
}


void Session_state::generate_client_side_info(Generator &g, Detail detail) const
{
	g.attribute("service", _service.name());
	g.attribute("label", _label);
	g.attribute("state", String<32>(Formatted_phase(phase)));
	g.attribute("ram", String<32>(_donated_ram_quota));
	g.attribute("caps", String<32>(_donated_cap_quota));

	if (detail.args == Detail::ARGS)
		g.node("args", [&] { g.append_quoted(_args.string()); });
}


void Session_state::generate_server_side_info(Generator &g, Detail detail) const
{
	generate_client_side_info(g, detail);
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
                             Session::Label     const &label,
                             Session::Diag      const  diag,
                             Args const               &args,
                             Affinity           const &affinity)
:
	_service(service),
	_donated_ram_quota(ram_quota_from_args(args.string())),
	_donated_cap_quota(cap_quota_from_args(args.string())),
	_id_at_client(*this, client_id_space, client_id),
	_label(label), _diag(diag), _args(args), _affinity(affinity)
{ }
