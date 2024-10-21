/*
 * \brief  Server role of init, forwarding session requests to children
 * \author Norman Feske
 * \date   2017-03-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/quota_transfer.h>
#include <os/session_policy.h>

/* local includes */
#include "server.h"


/******************************
 ** Sandbox::Server::Service **
 ******************************/

struct Sandbox::Server::Service : Service_model
{
	using Name = Genode::Service::Name;

	Name const _name;

	Registry<Service>::Element _registry_element;

	Allocator &_alloc;

	Registry<Routed_service> &_child_services;

	Constructible<Buffered_xml> _service_node { };

	/**
	 * Constructor
	 *
	 * \param alloc  allocator used for buffering the 'service_node'
	 */
	Service(Registry<Service>        &services,
	        Allocator                &alloc,
	        Xml_node                  service_node,
	        Registry<Routed_service> &child_services)
	:
		_name(service_node.attribute_value("name", Name())),
		_registry_element(services, *this),
		_alloc(alloc),
		_child_services(child_services)
	{ }

	/**
	 * Service_model interface
	 */
	void update_from_xml(Xml_node const &service_node) override
	{
		_service_node.construct(_alloc, service_node);
	}

	/**
	 * Service_model interface
	 */
	bool matches(Xml_node const &service_node) override
	{
		return _name == service_node.attribute_value("name", Name());
	}

	/**
	 * Determine route to child service for a given label according
	 * to the <service> node policy
	 *
	 * \throw Service_denied
	 */
	Route resolve_session_request(Session_label const &);

	Name name() const { return _name; }
};


Sandbox::Server::Route
Sandbox::Server::Service::resolve_session_request(Session_label const &label)
{
	if (!_service_node.constructed())
		throw Service_denied();

	try {
		Session_policy policy(label, _service_node->xml());

		if (!policy.has_sub_node("child"))
			throw Service_denied();

		Xml_node target_node = policy.sub_node("child");

		Child_policy::Name const child_name =
			target_node.attribute_value("name", Child_policy::Name());

		using Label = String<Session_label::capacity()>;
		Label const target_label =
			target_node.attribute_value("label", Label(label.string()));

		Routed_service *match = nullptr;
		_child_services.for_each([&] (Routed_service &service) {
			if (service.child_name() == child_name && service.name() == name())
				match = &service; });

		if (!match || match->abandoned())
			throw Service_not_present();

		return Route { *match, target_label };
	}
	catch (Session_policy::No_policy_defined) {
		throw Service_denied(); }
}


/*********************
 ** Sandbox::Server **
 *********************/

Sandbox::Server::Route
Sandbox::Server::_resolve_session_request(Service::Name const &service_name,
                                          Session_label const &label)
{
	Service *matching_service = nullptr;
	_services.for_each([&] (Service &service) {
		if (service.name() == service_name)
			matching_service = &service; });

	if (!matching_service)
		throw Service_not_present();

	return matching_service->resolve_session_request(label);
}


static void close_session(Genode::Session_state &session)
{
	session.phase = Genode::Session_state::CLOSE_REQUESTED;
	session.service().initiate_request(session);
	session.service().wakeup();
}


void Sandbox::Server::session_ready(Session_state &session)
{
	_report_update_trigger.trigger_report_update();

	/*
	 * If 'session_ready' is called as response to a session-quota upgrade,
	 * the 'phase' is set to 'CAP_HANDED_OUT' by 'Child::session_response'.
	 * We just need to forward the state change to our parent.
	 */
	if (session.phase == Session_state::CAP_HANDED_OUT) {
		Parent::Server::Id id { session.id_at_client().value };
		_env.parent().session_response(id, Parent::SESSION_OK);
	}

	if (session.phase == Session_state::AVAILABLE) {
		Parent::Server::Id id { session.id_at_client().value };
		_env.parent().deliver_session_cap(id, session.cap);
		session.phase = Session_state::CAP_HANDED_OUT;
	}

	if (session.phase == Session_state::SERVICE_DENIED)
		_close_session(session, Parent::SERVICE_DENIED);

	if (session.phase == Session_state::INSUFFICIENT_RAM_QUOTA)
		_close_session(session, Parent::INSUFFICIENT_RAM_QUOTA);

	if (session.phase == Session_state::INSUFFICIENT_CAP_QUOTA)
		_close_session(session, Parent::INSUFFICIENT_CAP_QUOTA);
}


void Sandbox::Server::_close_session(Session_state &session,
                                     Parent::Session_response response)
{
	_report_update_trigger.trigger_report_update();

	Ram_transfer::Account &service_ram_account = session.service();
	Cap_transfer::Account &service_cap_account = session.service();

	service_ram_account.try_transfer(_env.pd_session_cap(),
	                                 session.donated_ram_quota());

	service_cap_account.try_transfer(_env.pd_session_cap(),
	                                 session.donated_cap_quota());

	Parent::Server::Id id { session.id_at_client().value };

	session.destroy();

	_env.parent().session_response(id, response);
}


void Sandbox::Server::session_closed(Session_state &session)
{
	_close_session(session, Parent::SESSION_CLOSED);
}


void Sandbox::Server::_handle_create_session_request(Xml_node request,
                                                     Parent::Client::Id id)
{
	/*
	 * Ignore requests that are already successfully forwarded (by a prior call
	 * of '_handle_create_session_request') but still remain present in the
	 *  'session_requests' ROM because the server child has not responded yet.
	 */
	try {
		_client_id_space.apply<Parent::Client>(id, [&] (Parent::Client const &) { });
		return;
	} catch (Id_space<Parent::Client>::Unknown_id) { /* normal case */ }

	if (!request.has_sub_node("args"))
		return;

	using Args = Session_state::Args;
	Args const args = request.sub_node("args").decoded_content<Args>();

	Service::Name const name = request.attribute_value("service", Service::Name());

	Session_label const label = label_from_args(args.string());

	try {
		Route const route = _resolve_session_request(name, label);

		/*
		 * Reduce session quota by local session costs
		 */
		char argbuf[Parent::Session_args::MAX_SIZE];
		copy_cstring(argbuf, args.string(), sizeof(argbuf));

		Cap_quota const cap_quota = cap_quota_from_args(argbuf);
		Ram_quota const ram_quota = ram_quota_from_args(argbuf);

		size_t const keep_quota = route.service.factory().session_costs();

		if (ram_quota.value < keep_quota)
			throw Genode::Insufficient_ram_quota();

		Ram_quota const forward_ram_quota { ram_quota.value - keep_quota };

		Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", (int)forward_ram_quota.value);

		Session::Diag const diag = session_diag_from_args(args.string());

		Session_state &session =
			route.service.create_session(route.service.factory(),
			                             _client_id_space, id, route.label,
			                             diag, argbuf, Affinity::from_xml(request));

		/* transfer session quota */
		try {
			Ram_transfer::Remote_account env_ram_account(_env.pd(), _env.pd_session_cap());
			Cap_transfer::Remote_account env_cap_account(_env.pd(), _env.pd_session_cap());

			Ram_transfer ram_transfer(forward_ram_quota, env_ram_account, route.service);
			Cap_transfer cap_transfer(cap_quota,         env_cap_account, route.service);

			ram_transfer.acknowledge();
			cap_transfer.acknowledge();
		}
		catch (...) {
			/*
			 * This should never happen unless our parent missed to
			 * transfor the session quota to us prior issuing the session
			 * request.
			 */
			warning("unable to transfer session quota "
			        "(", ram_quota, " bytes, ", cap_quota, " caps) "
			        "of forwarded ", name, " session");
			session.destroy();
			throw Service_denied();
		}

		session.ready_callback  = this;
		session.closed_callback = this;

		/* initiate request */
		route.service.initiate_request(session);

		/* if request was not handled synchronously, kick off async operation */
		if (session.phase == Session_state::CREATE_REQUESTED)
			route.service.wakeup();

		if (session.phase == Session_state::SERVICE_DENIED)
			throw Service_denied();

		if (session.phase == Session_state::INSUFFICIENT_RAM_QUOTA)
			throw Insufficient_ram_quota();

		if (session.phase == Session_state::INSUFFICIENT_CAP_QUOTA)
			throw Insufficient_cap_quota();
	}
	catch (Service_denied) {
		_env.parent().session_response(Parent::Server::Id { id.value },
		                               Parent::SERVICE_DENIED); }
	catch (Insufficient_ram_quota) {
		_env.parent().session_response(Parent::Server::Id { id.value },
		                               Parent::INSUFFICIENT_RAM_QUOTA); }
	catch (Insufficient_cap_quota) {
		_env.parent().session_response(Parent::Server::Id { id.value },
		                               Parent::INSUFFICIENT_CAP_QUOTA); }
	catch (Service_not_present) { /* keep request pending */ }
}


void Sandbox::Server::_handle_upgrade_session_request(Xml_node request,
                                                      Parent::Client::Id id)
{
	_client_id_space.apply<Session_state>(id, [&] (Session_state &session) {

		if (session.phase == Session_state::UPGRADE_REQUESTED)
			return;

		Ram_quota const ram_quota { request.attribute_value("ram_quota", 0UL) };
		Cap_quota const cap_quota { request.attribute_value("cap_quota", 0UL) };

		try {
			Ram_transfer::Remote_account env_ram_account(_env.pd(), _env.pd_session_cap());
			Cap_transfer::Remote_account env_cap_account(_env.pd(), _env.pd_session_cap());

			Ram_transfer ram_transfer(ram_quota, env_ram_account, session.service());
			Cap_transfer cap_transfer(cap_quota, env_cap_account, session.service());

			ram_transfer.acknowledge();
			cap_transfer.acknowledge();
		}
		catch (...) {
			warning("unable to upgrade session quota "
			        "(", ram_quota, " bytes, ", cap_quota, " caps) "
			        "of forwarded ", session.service().name(), " session");
			return;
		}

		session.phase = Session_state::UPGRADE_REQUESTED;
		session.increase_donated_quota(ram_quota, cap_quota);
		session.service().initiate_request(session);
		session.service().wakeup();
	});
}


void Sandbox::Server::_handle_close_session_request(Xml_node, Parent::Client::Id id)
{
	_client_id_space.apply<Session_state>(id, [&] (Session_state &session) {
		close_session(session); });
}


void Sandbox::Server::_handle_session_request(Xml_node request)
{
	if (!request.has_attribute("id"))
		return;

	/*
	 * We use the 'Parent::Server::Id' of the incoming request as the
	 * 'Parent::Client::Id' of the forwarded request.
	 */
	Parent::Client::Id const id { request.attribute_value("id", 0UL) };

	if (request.has_type("create"))
		_handle_create_session_request(request, id);

	if (request.has_type("upgrade"))
		_handle_upgrade_session_request(request, id);

	if (request.has_type("close"))
		_handle_close_session_request(request, id);
}


void Sandbox::Server::_handle_session_requests()
{
	_session_requests->update();

	Xml_node const requests = _session_requests->xml();

	requests.for_each_sub_node([&] (Xml_node request) {
		_handle_session_request(request); });

	_report_update_trigger.trigger_report_update();
}


Sandbox::Service_model &Sandbox::Server::create_service(Xml_node const &node)
{
	return *new (_alloc) Service(_services, _alloc, node, _child_services);
}


void Sandbox::Server::destroy_service(Service_model &service)
{
	destroy(_alloc, &service);
}


void Sandbox::Server::apply_updated_policy()
{
	/*
	 * Construct mechanics for responding to our parent's session requests
	 * on demand.
	 */
	bool services_provided = false;
	_services.for_each([&] (Service const &) { services_provided = true; });

	if (services_provided && !_session_requests.constructed()) {
		_session_requests.construct(_env, "session_requests");
		_session_request_handler.construct(_env.ep(), *this,
		                                   &Server::_handle_session_requests);
		_session_requests->sigh(*_session_request_handler);
	}

	/*
	 * Try to resolve pending session requests that may become serviceable with
	 * the new configuration.
	 */
	if (services_provided && _session_requests.constructed())
		_handle_session_requests();

	/*
	 * Re-validate routes of existing sessions, close sessions whose routes
	 * changed.
	 */
	_client_id_space.for_each<Session_state>([&] (Session_state &session) {
		try {
			Route const route = _resolve_session_request(session.service().name(),
			                                             session.client_label());

			bool const route_unchanged = (route.service == session.service())
			                          && (route.label   == session.label());
			if (!route_unchanged)
				throw Service_denied();
		}
		catch (Service_denied)      { close_session(session); }
		catch (Service_not_present) { close_session(session); }
	});
}
