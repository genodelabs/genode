/*
 * \brief  Sandbox library
 * \author Norman Feske
 * \date   2020-01-10
 */

/*
 * Copyright (C) 2010-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <os/sandbox.h>

/* local includes */
#include <child.h>
#include <alias.h>
#include <server.h>
#include <heartbeat.h>

struct Genode::Sandbox::Library : ::Sandbox::State_reporter::Producer,
                                  ::Sandbox::Child::Default_route_accessor,
                                  ::Sandbox::Child::Default_caps_accessor,
                                  ::Sandbox::Child::Ram_limit_accessor,
                                  ::Sandbox::Child::Cap_limit_accessor
{
	using Routed_service = ::Sandbox::Routed_service;
	using Parent_service = ::Sandbox::Parent_service;
	using Local_service  = ::Genode::Sandbox::Local_service_base;
	using Report_detail  = ::Sandbox::Report_detail;
	using Child_registry = ::Sandbox::Child_registry;
	using Verbose        = ::Sandbox::Verbose;
	using State_reporter = ::Sandbox::State_reporter;
	using Heartbeat      = ::Sandbox::Heartbeat;
	using Server         = ::Sandbox::Server;
	using Alias          = ::Sandbox::Alias;
	using Child          = ::Sandbox::Child;
	using Prio_levels    = ::Sandbox::Prio_levels;

	Env  &_env;
	Heap &_heap;

	Registry<Parent_service>  _parent_services { };
	Registry<Routed_service>  _child_services  { };
	Registry<Local_service>  &_local_services;
	Child_registry            _children        { };

	Reconstructible<Verbose> _verbose { };

	Constructible<Buffered_xml> _default_route { };

	Cap_quota _default_caps { 0 };

	unsigned _child_cnt = 0;

	static Ram_quota _preserved_ram_from_config(Xml_node config)
	{
		Number_of_bytes preserve { 40*sizeof(long)*1024 };

		config.for_each_sub_node("resource", [&] (Xml_node node) {
			if (node.attribute_value("name", String<16>()) == "RAM")
				preserve = node.attribute_value("preserve", preserve); });

		return Ram_quota { preserve };
	}

	Ram_quota _preserved_ram  { 0 };
	Cap_quota _preserved_caps { 0 };

	Ram_quota _avail_ram() const
	{
		Ram_quota avail_ram = _env.pd().avail_ram();

		if (_preserved_ram.value > avail_ram.value) {
			error("RAM preservation exceeds available memory");
			return Ram_quota { 0 };
		}

		/* deduce preserved quota from available quota */
		return Ram_quota { avail_ram.value - _preserved_ram.value };
	}

	static Cap_quota _preserved_caps_from_config(Xml_node config)
	{
		size_t preserve = 20;

		config.for_each_sub_node("resource", [&] (Xml_node node) {
			if (node.attribute_value("name", String<16>()) == "CAP")
				preserve = node.attribute_value("preserve", preserve); });

		return Cap_quota { preserve };
	}

	Cap_quota _avail_caps() const
	{
		Cap_quota avail_caps { _env.pd().avail_caps().value };

		if (_preserved_caps.value > avail_caps.value) {
			error("Capability preservation exceeds available capabilities");
			return Cap_quota { 0 };
		}

		/* deduce preserved quota from available quota */
		return Cap_quota { avail_caps.value - _preserved_caps.value };
	}

	/**
	 * Child::Ram_limit_accessor interface
	 */
	Ram_quota resource_limit(Ram_quota const &) const override { return _avail_ram(); }

	/**
	 * Child::Cap_limit_accessor interface
	 */
	Cap_quota resource_limit(Cap_quota const &) const override { return _avail_caps(); }

	void _handle_resource_avail() { }

	void produce_state_report(Xml_generator &xml, Report_detail const &detail) const override
	{
		if (detail.init_ram())
			xml.node("ram",  [&] () { ::Sandbox::generate_ram_info (xml, _env.pd()); });

		if (detail.init_caps())
			xml.node("caps", [&] () { ::Sandbox::generate_caps_info(xml, _env.pd()); });

		if (detail.children())
			_children.report_state(xml, detail);
	}

	/**
	 * Default_route_accessor interface
	 */
	Xml_node default_route() override
	{
		return _default_route.constructed() ? _default_route->xml()
		                                    : Xml_node("<empty/>");
	}

	/**
	 * Default_caps_accessor interface
	 */
	Cap_quota default_caps() override { return _default_caps; }

	State_reporter _state_reporter;

	Heartbeat _heartbeat { _env, _children, _state_reporter };

	void _update_aliases_from_config(Xml_node const &);
	void _update_parent_services_from_config(Xml_node const &);
	void _abandon_obsolete_children(Xml_node const &);
	void _update_children_config(Xml_node const &);
	void _destroy_abandoned_parent_services();

	Server _server { _env, _heap, _child_services, _state_reporter };

	Library(Env &env, Heap &heap, Registry<Local_service> &local_services,
	        State_handler &state_handler)
	:
		_env(env), _heap(heap), _local_services(local_services),
		_state_reporter(_env, *this, state_handler)
	{ }

	void apply_config(Xml_node const &);

	void generate_state_report(Xml_generator &xml) const
	{
		_state_reporter.generate(xml);
	}
};


void Genode::Sandbox::Library::_update_parent_services_from_config(Xml_node const &config)
{
	Xml_node const node = config.has_sub_node("parent-provides")
	                    ? config.sub_node("parent-provides")
	                    : Xml_node("<empty/>");

	/* remove services that are no longer present in config */
	_parent_services.for_each([&] (Parent_service &service) {

		Service::Name const name = service.name();

		bool obsolete = true;
		node.for_each_sub_node("service", [&] (Xml_node service) {
			if (name == service.attribute_value("name", Service::Name())) {
				obsolete = false; }});

		if (obsolete)
			service.abandon();
	});

	/* used to prepend the list of new parent services with title */
	bool first_log = true;

	/* register new services */
	node.for_each_sub_node("service", [&] (Xml_node service) {

		Service::Name const name = service.attribute_value("name", Service::Name());

		bool registered = false;
		_parent_services.for_each([&] (Parent_service const &service) {
			if (service.name() == name)
				registered = true; });

		if (!registered) {
			new (_heap) ::Sandbox::Parent_service(_parent_services, _env, name);
			if (_verbose->enabled()) {
				if (first_log)
					log("parent provides");
				log("  service \"", name, "\"");
				first_log = false;
			}
		}
	});
}


void Genode::Sandbox::Library::_destroy_abandoned_parent_services()
{
	_parent_services.for_each([&] (Parent_service &service) {
		if (service.abandoned())
			destroy(_heap, &service); });
}


void Genode::Sandbox::Library::_update_aliases_from_config(Xml_node const &config)
{
	/* remove all known aliases */
	while (_children.any_alias()) {
		::Sandbox::Alias *alias = _children.any_alias();
		_children.remove_alias(alias);
		destroy(_heap, alias);
	}

	/* create aliases */
	config.for_each_sub_node("alias", [&] (Xml_node alias_node) {

		try {
			_children.insert_alias(new (_heap) Alias(alias_node)); }
		catch (Alias::Name_is_missing) {
			warning("missing 'name' attribute in '<alias>' entry"); }
		catch (Alias::Child_is_missing) {
			warning("missing 'child' attribute in '<alias>' entry"); }
	});
}


void Genode::Sandbox::Library::_abandon_obsolete_children(Xml_node const &config)
{
	_children.for_each_child([&] (Child &child) {

		bool obsolete = true;
		config.for_each_sub_node("start", [&] (Xml_node node) {
			if (child.has_name   (node.attribute_value("name", Child_policy::Name()))
			 && child.has_version(node.attribute_value("version", Child::Version())))
				obsolete = false; });

		if (obsolete)
			child.abandon();
	});
}


void Genode::Sandbox::Library::_update_children_config(Xml_node const &config)
{
	for (;;) {

		/*
		 * Children are abandoned if any of their client sessions can no longer
		 * be routed or result in a different route. As each child may be a
		 * service, an avalanche effect may occur. It stops if no update causes
		 * a potential side effect in one iteration over all chilren.
		 */
		bool side_effects = false;

		config.for_each_sub_node("start", [&] (Xml_node node) {

			Child_policy::Name const start_node_name =
				node.attribute_value("name", Child_policy::Name());

			_children.for_each_child([&] (Child &child) {
				if (!child.abandoned() && child.name() == start_node_name) {
					switch (child.apply_config(node)) {
					case Child::NO_SIDE_EFFECTS: break;
					case Child::MAY_HAVE_SIDE_EFFECTS: side_effects = true; break;
					};
				}
			});
		});

		if (!side_effects)
			break;
	}
}


void Genode::Sandbox::Library::apply_config(Xml_node const &config)
{
	bool update_state_report = false;

	_preserved_ram  = _preserved_ram_from_config(config);
	_preserved_caps = _preserved_caps_from_config(config);

	_verbose.construct(config);
	_state_reporter.apply_config(config);
	_heartbeat.apply_config(config);

	/* determine default route for resolving service requests */
	try {
		_default_route.construct(_heap, config.sub_node("default-route")); }
	catch (...) { }

	_default_caps = Cap_quota { 0 };
	try {
		_default_caps = Cap_quota { config.sub_node("default")
		                                  .attribute_value("caps", 0UL) }; }
	catch (...) { }

	Prio_levels     const prio_levels    = ::Sandbox::prio_levels_from_xml(config);
	Affinity::Space const affinity_space = ::Sandbox::affinity_space_from_xml(config);
	bool            const space_defined  = config.has_sub_node("affinity-space");

	_update_aliases_from_config(config);
	_update_parent_services_from_config(config);
	_abandon_obsolete_children(config);
	_update_children_config(config);

	/* kill abandoned children */
	_children.for_each_child([&] (Child &child) {

		if (!child.abandoned())
			return;

		/* make the child's services unavailable */
		child.destroy_services();
		child.close_all_sessions();
		update_state_report = true;

		/* destroy child once all environment sessions are gone */
		if (child.env_sessions_closed()) {
			_children.remove(&child);
			destroy(_heap, &child);
		}
	});

	_destroy_abandoned_parent_services();

	/* initial RAM and caps limit before starting new children */
	Ram_quota const avail_ram  = _avail_ram();
	Cap_quota const avail_caps = _avail_caps();

	/* variable used to track the RAM and caps taken by new started children */
	Ram_quota used_ram  { 0 };
	Cap_quota used_caps { 0 };

	/* create new children */
	try {
		config.for_each_sub_node("start", [&] (Xml_node start_node) {

			bool exists = false;

			unsigned num_abandoned = 0;

			typedef Child_policy::Name Name;
			Name const child_name = start_node.attribute_value("name", Name());

			_children.for_each_child([&] (Child const &child) {
				if (child.name() == child_name) {
					if (child.abandoned())
						num_abandoned++;
					else
						exists = true;
				}
			});

			/* skip start node if corresponding child already exists */
			if (exists)
				return;

			/* prevent queuing up abandoned children with the same name */
			if (num_abandoned > 1)
				return;

			if (used_ram.value > avail_ram.value) {
				error("RAM exhausted while starting child: ", child_name);
				return;
			}

			if (used_caps.value > avail_caps.value) {
				error("capabilities exhausted while starting child: ", child_name);
				return;
			}

			if (!space_defined && start_node.has_sub_node("affinity")) {
				warning("affinity-space configuration missing, "
				        "but affinity defined for child: ", child_name);
			}

			try {
				Child &child = *new (_heap)
					Child(_env, _heap, *_verbose,
					      Child::Id { ++_child_cnt }, _state_reporter,
					      start_node, *this, *this, _children,
					      Ram_quota { avail_ram.value  - used_ram.value },
					      Cap_quota { avail_caps.value - used_caps.value },
					       *this, *this, prio_levels, affinity_space,
					      _parent_services, _child_services, _local_services);
				_children.insert(&child);

				update_state_report = true;

				/* account for the start XML node buffered in the child */
				size_t const metadata_overhead = start_node.size() + sizeof(Child);

				/* track used memory and RAM limit */
				used_ram = Ram_quota { used_ram.value
				                     + child.ram_quota().value
				                     + metadata_overhead };

				used_caps = Cap_quota { used_caps.value
				                      + child.cap_quota().value };
			}
			catch (Rom_connection::Rom_connection_failed) {
				/*
				 * The binary does not exist. An error message is printed
				 * by the Rom_connection constructor.
				 */
			}
			catch (Out_of_ram) {
				warning("memory exhausted during child creation"); }
			catch (Out_of_caps) {
				warning("local capabilities exhausted during child creation"); }
			catch (Child::Missing_name_attribute) {
				warning("skipped startup of nameless child"); }
			catch (Region_map::Region_conflict) {
				warning("failed to attach dataspace to local address space "
				        "during child construction"); }
			catch (Region_map::Invalid_dataspace) {
				warning("attempt to attach invalid dataspace to local address space "
				        "during child construction"); }
			catch (Service_denied) {
				warning("failed to create session during child construction"); }
		});
	}
	catch (Xml_node::Nonexistent_sub_node) { error("no children to start"); }
	catch (Xml_node::Invalid_syntax) { error("config has invalid syntax"); }
	catch (Child_registry::Alias_name_is_not_unique) { }

	/*
	 * Initiate RAM sessions of all new children
	 */
	_children.for_each_child([&] (Child &child) {
		if (!child.abandoned())
			child.initiate_env_pd_session(); });

	/*
	 * Initiate remaining environment sessions of all new children
	 */
	_children.for_each_child([&] (Child &child) {
		if (!child.abandoned())
			child.initiate_env_sessions(); });

	/*
	 * (Re-)distribute RAM and capability quota among the children, given their
	 * resource assignments and the available slack memory. We first apply
	 * possible downgrades to free as much resources as we can. These resources
	 * are then incorporated in the subsequent upgrade step.
	 */
	_children.for_each_child([&] (Child &child) { child.apply_downgrade(); });
	_children.for_each_child([&] (Child &child) { child.apply_upgrade(); });

	_server.apply_config(config);

	if (update_state_report)
		_state_reporter.trigger_immediate_report_update();
}


/*********************************
 ** Sandbox::Local_service_base **
 *********************************/

void Genode::Sandbox::Local_service_base::_for_each_requested_session(Request_fn &fn)
{
	_server_id_space.for_each<Session_state>([&] (Session_state &session) {

		if (session.phase == Session_state::CREATE_REQUESTED) {

			Request request(session);

			fn.with_requested_session(request);

			bool wakeup_client = false;

			if (request._denied) {
				session.phase = Session_state::SERVICE_DENIED;
				wakeup_client = true;
			}

			if (request._session_ptr) {
				session.local_ptr = request._session_ptr;
				session.cap       = request._session_cap;
				session.phase     = Session_state::AVAILABLE;
				wakeup_client     = true;
			}

			if (wakeup_client && session.ready_callback)
				session.ready_callback->session_ready(session);
		}
	});
}


void Genode::Sandbox::Local_service_base::_for_each_upgraded_session(Upgrade_fn &fn)
{
	_server_id_space.for_each<Session_state>([&] (Session_state &session) {

		if (session.phase != Session_state::UPGRADE_REQUESTED)
			return;

		if (session.local_ptr == nullptr)
			return;

		bool wakeup_client = false;

		Session::Resources const amount { session.ram_upgrade,
		                                  session.cap_upgrade };

		switch (fn.with_upgraded_session(*session.local_ptr, amount)) {

			case Upgrade_response::CONFIRMED:
				session.phase = Session_state::CAP_HANDED_OUT;
				wakeup_client = true;
				break;

			case Upgrade_response::DEFERRED:
				break;
			}

		if (wakeup_client && session.ready_callback)
			session.ready_callback->session_ready(session);
	});
}


void Genode::Sandbox::Local_service_base::_for_each_session_to_close(Close_fn &close_fn)
{
	/*
	 * Collection of closed sessions to be destructed via callback
	 *
	 * For asynchronous sessions, the 'Session_state' object is destructed by
	 * the 'Closed_callback'. We cannot issue the callback from within
	 * '_server_id_space.for_each()' because the destruction of 'id_at_server'
	 * would deadlock. Instead be collect the 'Session_state' objects to be
	 * called back in the 'pending_callbacks' ID space. This is possible
	 * because the parent ID space is not used for local services.
	 */
	Id_space<Parent::Client> pending_callbacks { };

	_server_id_space.for_each<Session_state>([&] (Session_state &session) {

		if (session.phase != Session_state::CLOSE_REQUESTED)
			return;

		if (session.local_ptr == nullptr)
			return;

		switch (close_fn.close_session(*session.local_ptr)) {

		case Close_response::CLOSED:

			session.phase = Session_state::CLOSED;

			if (session.closed_callback)
				session.id_at_parent.construct(session, pending_callbacks);
			break;

		case Close_response::DEFERRED:
			break;
		}
	});

	/*
	 * Purge 'Session_state' objects by calling 'closed_callback'
	 */
	while (pending_callbacks.apply_any<Session_state>([&] (Session_state &session) {

		session.id_at_parent.destruct();

		if (session.closed_callback)
			session.closed_callback->session_closed(session);
	}));
}


Genode::Sandbox::Local_service_base::Local_service_base(Sandbox    &sandbox,
                                                        Name const &name,
                                                        Wakeup     &wakeup)
:
	Service(name),
	_element(sandbox._local_services, *this),
	_session_factory(sandbox._heap, Session_state::Factory::Batch_size{16}),
	_async_wakeup(wakeup),
	_async_service(name, _server_id_space, _session_factory, _async_wakeup)
{ }


/*************
 ** Sandbox **
 *************/

void Genode::Sandbox::apply_config(Xml_node const &config)
{
	_library.apply_config(config);
}


void Genode::Sandbox::generate_state_report(Xml_generator &xml) const
{
	_library.generate_state_report(xml);
}


Genode::Sandbox::Sandbox(Env &env, State_handler &state_handler)
:
	_heap(env.ram(), env.rm()),
	_library(*new (_heap) Library(env, _heap, _local_services, state_handler))
{ }

