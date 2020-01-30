/*
 * \brief  Child representation
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <vm_session/vm_session.h>

/* local includes */
#include <child.h>


void Sandbox::Child::destroy_services()
{
	_child_services.for_each([&] (Routed_service &service) {
		if (service.has_id_space(_session_requester.id_space()))
			destroy(_alloc, &service); });
}


Sandbox::Child::Apply_config_result
Sandbox::Child::apply_config(Xml_node start_node)
{
	if (_state == STATE_ABANDONED || _exited)
		return NO_SIDE_EFFECTS;

	/*
	 * If the child's environment is incomplete, restart it to attempt
	 * the re-routing of its environment sessions.
	 */
	{
		bool env_log_exists = false, env_binary_exists = false;
		_child.for_each_session([&] (Session_state const &session) {
			Parent::Client::Id const id = session.id_at_client();
			env_log_exists    |= (id == Parent::Env::log());
			env_binary_exists |= (id == Parent::Env::binary());
		});

		if (!env_binary_exists || !env_log_exists) {
			abandon();
			return MAY_HAVE_SIDE_EFFECTS;
		}
	}

	bool provided_services_changed = false;

	enum Config_update { CONFIG_APPEARED, CONFIG_VANISHED,
	                     CONFIG_CHANGED,  CONFIG_UNCHANGED };

	Config_update config_update = CONFIG_UNCHANGED;

	/*
	 * Import new start node if it differs
	 */
	if (start_node.differs_from(_start_node->xml())) {

		/*
		 * Start node changed
		 *
		 * Determine how the inline config is affected.
		 */
		char const * const tag = "config";
		bool const config_was_present = _start_node->xml().has_sub_node(tag);
		bool const config_is_present  = start_node.has_sub_node(tag);

		if (config_was_present && !config_is_present)
			config_update = CONFIG_VANISHED;

		if (!config_was_present && config_is_present)
			config_update = CONFIG_APPEARED;

		if (config_was_present && config_is_present) {

			Xml_node const old_config = _start_node->xml().sub_node(tag);
			Xml_node const new_config = start_node.sub_node(tag);

			if (new_config.differs_from(old_config))
				config_update = CONFIG_CHANGED;
		}

		/*
		 * Import updated <provides> node
		 *
		 * First abandon services that are no longer present in the
		 * <provides> node. Then add services that have newly appeared.
		 */
		_child_services.for_each([&] (Routed_service &service) {

			if (!_provided_by_this(service))
				return;

			typedef Service::Name Name;
			Name const name = service.name();

			bool still_provided = false;
			_provides_sub_node(start_node)
				.for_each_sub_node("service", [&] (Xml_node node) {
					if (name == node.attribute_value("name", Name()))
						still_provided = true; });

			if (!still_provided) {
				service.abandon();
				provided_services_changed = true;
			}
		});

		_provides_sub_node(start_node).for_each_sub_node("service",
		                                                 [&] (Xml_node node) {
			if (_service_exists(node))
				return;

			_add_service(node);
			provided_services_changed = true;
		});

		/*
		 * Import new binary name. A change may affect the route for
		 * the binary's ROM session, triggering the restart of the
		 * child.
		 */
		_binary_name = _binary_from_xml(start_node, _unique_name);

		_heartbeat_enabled = start_node.has_sub_node("heartbeat");

		/* import new start node */
		_start_node.construct(_alloc, start_node);
	}

	/*
	 * Apply change to '_config_rom_service'. This will
	 * potentially result in a change of the "config" ROM route, which
	 * may in turn prompt the routing-check below to abandon (restart)
	 * the child.
	 */
	switch (config_update) {
	case CONFIG_UNCHANGED:                                       break;
	case CONFIG_CHANGED:  _config_rom_service->trigger_update(); break;
	case CONFIG_APPEARED: _config_rom_service.construct(*this);  break;
	case CONFIG_VANISHED: _config_rom_service->abandon();        break;
	}

	/* validate that the routes of all existing sessions remain intact */
	{
		bool routing_changed = false;
		_child.for_each_session([&] (Session_state const &session) {
			if (!_route_valid(session))
				routing_changed = true; });

		if (routing_changed) {
			abandon();
			return MAY_HAVE_SIDE_EFFECTS;
		}
	}

	if (provided_services_changed)
		return MAY_HAVE_SIDE_EFFECTS;

	return NO_SIDE_EFFECTS;
}


Sandbox::Ram_quota Sandbox::Child::_configured_ram_quota() const
{
	size_t assigned = 0;

	_start_node->xml().for_each_sub_node("resource", [&] (Xml_node resource) {
		if (resource.attribute_value("name", String<8>()) == "RAM")
			assigned = resource.attribute_value("quantum", Number_of_bytes()); });

	return Ram_quota { assigned };
}


Sandbox::Cap_quota Sandbox::Child::_configured_cap_quota() const
{
	size_t const default_caps = _default_caps_accessor.default_caps().value;

	return Cap_quota { _start_node->xml().attribute_value("caps", default_caps) };
}


template <typename QUOTA, typename LIMIT_ACCESSOR>
void Sandbox::Child::_apply_resource_upgrade(QUOTA &assigned, QUOTA const configured,
                                             LIMIT_ACCESSOR const &limit_accessor)
{
	if (configured.value <= assigned.value)
		return;

	QUOTA  const limit     = limit_accessor.resource_limit(QUOTA{});
	size_t const increment = configured.value - assigned.value;

	/*
	 * If the configured quota exceeds our own quota, we donate all remaining
	 * quota to the child.
	 */
	if (increment > limit.value)
		if (_verbose.enabled())
			warn_insuff_quota(limit.value);

	QUOTA const transfer { min(increment, limit.value) };

	/*
	 * Remember assignment and apply upgrade to child
	 *
	 * Note that we remember the actually transferred amount as the assigned
	 * amount. In the case where the value is clamped to to the limit, the
	 * value as given in the config remains diverged from the assigned value.
	 * This way, a future config update will attempt the completion of the
	 * upgrade if memory become available.
	 */
	if (transfer.value) {

		assigned.value += transfer.value;

		ref_pd().transfer_quota(_child.pd_session_cap(), transfer);

		/* wake up child that blocks on a resource request */
		if (_requested_resources.constructed()) {
			_child.notify_resource_avail();
			_requested_resources.destruct();
		}
	}
}


void Sandbox::Child::apply_upgrade()
{
	/* pd_session_cap of exited child is invalid and unusable for transfers */
	if (_exited)
		return;

	if (_resources.effective_ram_quota().value == 0)
		warning(name(), ": no valid RAM quota defined");

	_apply_resource_upgrade(_resources.assigned_ram_quota,
	                        _configured_ram_quota(), _ram_limit_accessor);

	if (_resources.effective_cap_quota().value == 0)
		warning(name(), ": no valid capability quota defined");

	_apply_resource_upgrade(_resources.assigned_cap_quota,
	                        _configured_cap_quota(), _cap_limit_accessor);
}


template <typename QUOTA, typename CHILD_AVAIL_QUOTA_FN>
void Sandbox::Child::_apply_resource_downgrade(QUOTA &assigned, QUOTA const configured,
                                               QUOTA const preserved,
                                               CHILD_AVAIL_QUOTA_FN const &child_avail_quota_fn)
{
	if (configured.value >= assigned.value)
		return;

	QUOTA const decrement { assigned.value - configured.value };

	/*
	 * The child may concurrently consume quota from its PD session,
	 * causing the 'transfer_quota' to fail. For this reason, we repeatedly
	 * attempt the transfer.
	 */
	unsigned max_attempts = 4, attempts = 0;
	for (; attempts < max_attempts; attempts++) {

		/* give up if the child's available quota is exhausted */
		size_t const avail = child_avail_quota_fn().value;
		if (avail < preserved.value)
			break;

		QUOTA const transfer { min(avail - preserved.value, decrement.value) };

		try {
			_child.pd().transfer_quota(ref_pd_cap(), transfer);
			assigned.value -= transfer.value;
			break;
		} catch (...) { }
	}

	if (attempts == max_attempts)
		warning(name(), ": downgrade failed after ", max_attempts, " attempts");
}


void Sandbox::Child::apply_downgrade()
{
	Ram_quota const configured_ram_quota = _configured_ram_quota();
	Cap_quota const configured_cap_quota = _configured_cap_quota();

	_apply_resource_downgrade(_resources.assigned_ram_quota,
	                          configured_ram_quota, Ram_quota{16*1024},
	                          [&] () { return _child.pd().avail_ram(); });

	_apply_resource_downgrade(_resources.assigned_cap_quota,
	                          configured_cap_quota, Cap_quota{5},
	                          [&] () { return _child.pd().avail_caps(); });

	/*
	 * If designated resource quota is lower than the child's consumed quota,
	 * issue a yield request to the child.
	 */
	size_t demanded_ram_quota = 0;
	size_t demanded_cap_quota = 0;

	if (configured_ram_quota.value < _resources.assigned_ram_quota.value)
		demanded_ram_quota = _resources.assigned_ram_quota.value - configured_ram_quota.value;

	if (configured_cap_quota.value < _resources.assigned_cap_quota.value)
		demanded_cap_quota = _resources.assigned_cap_quota.value - configured_cap_quota.value;

	if (demanded_ram_quota || demanded_cap_quota) {
		Parent::Resource_args const
			args { "ram_quota=", Number_of_bytes(demanded_ram_quota), ", ",
			       "cap_quota=", demanded_cap_quota};
		_child.yield(args);
	}
}


void Sandbox::Child::report_state(Xml_generator &xml, Report_detail const &detail) const
{
	if (abandoned())
		return;

	/* true if it's safe to call the PD for requesting resource information */
	bool const pd_alive = !abandoned() && !_exited;

	xml.node("child", [&] () {

		xml.attribute("name",   _unique_name);
		xml.attribute("binary", _binary_name);

		if (_version.valid())
			xml.attribute("version", _version);

		if (detail.ids())
			xml.attribute("id", _id.value);

		if (!_child.active())
			xml.attribute("state", "incomplete");

		if (_exited)
			xml.attribute("exited", _exit_value);

		if (_heartbeat_enabled && _child.skipped_heartbeats())
			xml.attribute("skipped_heartbeats", _child.skipped_heartbeats());

		if (detail.child_ram() && _child.pd_session_cap().valid()) {
			xml.node("ram", [&] () {

				xml.attribute("assigned", String<32> {
					Number_of_bytes(_resources.assigned_ram_quota.value) });

				if (pd_alive)
					generate_ram_info(xml, _child.pd());

				if (_requested_resources.constructed() && _requested_resources->ram.value)
					xml.attribute("requested", String<32>(_requested_resources->ram));
			});
		}

		if (detail.child_caps() && _child.pd_session_cap().valid()) {
			xml.node("caps", [&] () {

				xml.attribute("assigned", String<32>(_resources.assigned_cap_quota));

				if (pd_alive)
					generate_caps_info(xml, _child.pd());

				if (_requested_resources.constructed() && _requested_resources->caps.value)
					xml.attribute("requested", String<32>(_requested_resources->caps));
			});
		}

		Session_state::Detail const
			session_detail { detail.session_args() ? Session_state::Detail::ARGS
			                                       : Session_state::Detail::NO_ARGS};

		if (detail.requested()) {
			xml.node("requested", [&] () {
				_child.for_each_session([&] (Session_state const &session) {
					xml.node("session", [&] () {
						session.generate_client_side_info(xml, session_detail); }); }); });
		}

		if (detail.provided()) {
			xml.node("provided", [&] () {

				auto fn = [&] (Session_state const &session) {
					xml.node("session", [&] () {
						session.generate_server_side_info(xml, session_detail); }); };

				_session_requester.id_space().for_each<Session_state const>(fn);
			});
		}
	});
}


void Sandbox::Child::init(Pd_session &session, Pd_session_capability cap)
{
	session.ref_account(_env.pd_session_cap());

	size_t const initial_session_costs =
		session_alloc_batch_size()*_child.session_factory().session_costs();

	Ram_quota const ram_quota { _resources.effective_ram_quota().value > initial_session_costs
	                          ? _resources.effective_ram_quota().value - initial_session_costs
	                          : 0 };

	Cap_quota const cap_quota { _resources.effective_cap_quota().value };

	try { _env.pd().transfer_quota(cap, cap_quota); }
	catch (Out_of_caps) {
		error(name(), ": unable to initialize cap quota of PD"); }

	try { _env.pd().transfer_quota(cap, ram_quota); }
	catch (Out_of_ram) {
		error(name(), ": unable to initialize RAM quota of PD"); }
}


void Sandbox::Child::init(Cpu_session &session, Cpu_session_capability cap)
{
	static size_t avail = Cpu_session::quota_lim_upscale(                    100, 100);
	size_t const   need = Cpu_session::quota_lim_upscale(_resources.cpu_quota_pc, 100);
	size_t need_adj = 0;

	if (need > avail || avail == 0) {
		warn_insuff_quota(Cpu_session::quota_lim_downscale(avail, 100));
		need_adj = Cpu_session::quota_lim_upscale(100, 100);
		avail    = 0;
	} else {
		need_adj = Cpu_session::quota_lim_upscale(need, avail);
		avail   -= need;
	}
	session.ref_account(_env.cpu_session_cap());
	_env.cpu().transfer_quota(cap, need_adj);
}


Sandbox::Child::Route
Sandbox::Child::resolve_session_request(Service::Name const &service_name,
                                        Session_label const &label)
{
	/* check for "config" ROM request */
	if (service_name == Rom_session::service_name() &&
	    label.last_element() == "config") {

		if (_config_rom_service.constructed() &&
		   !_config_rom_service->abandoned())
			return Route { _config_rom_service->service(), label,
			               Session::Diag{false} };

		/*
		 * \deprecated  the support for the <configfile> tag will
		 *              be removed
		 */
		if (_start_node->xml().has_sub_node("configfile")) {

			typedef String<50> Name;
			Name const rom =
				_start_node->xml().sub_node("configfile")
				                  .attribute_value("name", Name());

			/* prevent infinite recursion */
			if (rom == "config") {
				error("configfile must not be named 'config'");
				throw Service_denied();
			}

			return resolve_session_request(service_name,
			                               prefixed_label(name(), rom));
		}

		/*
		 * If there is neither an inline '<config>' nor a
		 * '<configfile>' node present, we apply the regular session
		 * routing to the "config" ROM request.
		 */
	}

	/*
	 * Check for the binary's ROM request
	 *
	 * The binary is requested as a ROM with the child's unique
	 * name ('Child_policy::binary_name' equals 'Child_policy::name').
	 * If the binary name differs from the child's unique name,
	 * we resolve the session request with the binary name as label.
	 * Otherwise the regular routing is applied.
	 */
	if (service_name == Rom_session::service_name() &&
	    label == _unique_name && _unique_name != _binary_name)
		return resolve_session_request(service_name, _binary_name);

	/* supply binary as dynamic linker if '<start ld="no">' */
	if (!_use_ld && service_name == Rom_session::service_name() && label == "ld.lib.so")
		return resolve_session_request(service_name, _binary_name);

	/* check for "session_requests" ROM request */
	if (service_name == Rom_session::service_name()
	 && label.last_element() == Session_requester::rom_name())
		return Route { _session_requester.service(),
		               Session::Label(), Session::Diag{false} };

	try {
		Xml_node route_node = _default_route_accessor.default_route();
		try {
			route_node = _start_node->xml().sub_node("route"); }
		catch (...) { }
		Xml_node service_node = route_node.sub_node();

		for (; ; service_node = service_node.next()) {

			bool service_wildcard = service_node.has_type("any-service");

			if (!service_node_matches(service_node, label, name(), service_name))
				continue;

			Xml_node target = service_node.sub_node();
			for (; ; target = target.next()) {

				/*
				 * Determine session label to be provided to the server
				 *
				 * By default, the client's identity (accompanied with the a
				 * client-provided label) is presented as session label to the
				 * server. However, the target node can explicitly override the
				 * client's identity by a custom label via the 'label'
				 * attribute.
				 */
				typedef String<Session_label::capacity()> Label;
				Label const target_label =
					target.attribute_value("label", Label(label.string()));

				Session::Diag const
					target_diag { target.attribute_value("diag", false) };

				auto no_filter = [] (Service &) -> bool { return false; };

				if (target.has_type("parent")) {

					try {
						return Route { find_service(_parent_services, service_name, no_filter),
						               target_label, target_diag };
					} catch (Service_denied) { }
				}

				if (target.has_type("local")) {

					try {
						return Route { find_service(_local_services, service_name, no_filter),
						               target_label, target_diag };
					} catch (Service_denied) { }
				}

				if (target.has_type("child")) {

					typedef Name_registry::Name Name;
					Name server_name = target.attribute_value("name", Name());
					server_name = _name_registry.deref_alias(server_name);

					auto filter_server_name = [&] (Routed_service &s) -> bool {
						return s.child_name() != server_name; };

					try {
						return Route { find_service(_child_services, service_name, filter_server_name),
						               target_label, target_diag };

					} catch (Service_denied) { }
				}

				if (target.has_type("any-child")) {

					if (is_ambiguous(_child_services, service_name)) {
						error(name(), ": ambiguous routes to "
						      "service \"", service_name, "\"");
						throw Service_denied();
					}
					try {
						return Route { find_service(_child_services, service_name, no_filter),
						               target_label, target_diag };

					} catch (Service_denied) { }
				}

				if (!service_wildcard) {
					warning(name(), ": lookup for service \"", service_name, "\" failed");
					throw Service_denied();
				}

				if (target.last())
					break;
			}
		}
	} catch (Xml_node::Nonexistent_sub_node) { }

	warning(name(), ": no route to service \"", service_name, "\" (label=\"", label, "\")");
	throw Service_denied();
}


void Sandbox::Child::filter_session_args(Service::Name const &service,
                                         char *args, size_t args_len)
{
	/*
	 * Intercept CPU session requests to scale priorities
	 */
	if ((service == Cpu_session::service_name() ||
	     service == Vm_session::service_name())
	    && _prio_levels_log2 > 0) {

		unsigned long priority = Arg_string::find_arg(args, "priority").ulong_value(0);

		/* clamp priority value to valid range */
		priority = min((unsigned)Cpu_session::PRIORITY_LIMIT - 1, priority);

		long discarded_prio_lsb_bits_mask = (1 << _prio_levels_log2) - 1;
		if (priority & discarded_prio_lsb_bits_mask)
			warning("priority band too small, losing least-significant priority bits");

		priority >>= _prio_levels_log2;

		/* assign child priority to the most significant priority bits */
		priority |= _priority*(Cpu_session::PRIORITY_LIMIT >> _prio_levels_log2);

		/* override priority when delegating the session request to the parent */
		String<64> value { Hex(priority) };
		Arg_string::set_arg(args, args_len, "priority", value.string());
	}

	/*
	 * Remove phys_start and phys_size RAM-session arguments unless
	 * explicitly permitted by the child configuration.
	 */
	if (service == Pd_session::service_name()) {

		/*
		 * If the child is allowed to constrain physical memory allocations,
		 * pass the child-provided constraints as session arguments to core.
		 * If no constraints are specified, we apply the constraints for
		 * allocating DMA memory (as the only use case for the constrain-phys
		 * mechanism).
		 */
		if (_constrain_phys) {
			addr_t start = 0;
			addr_t size  = (sizeof(long) == 4) ? 0xc0000000UL : 0x100000000UL;

			Arg_string::find_arg(args, "phys_start").ulong_value(start);
			Arg_string::find_arg(args, "phys_size") .ulong_value(size);

			Arg_string::set_arg(args, args_len, "phys_start", String<32>(Hex(start)).string());
			Arg_string::set_arg(args, args_len, "phys_size",  String<32>(Hex(size)) .string());
		} else {
			Arg_string::remove_arg(args, "phys_start");
			Arg_string::remove_arg(args, "phys_size");
		}
	}
}


Genode::Affinity Sandbox::Child::filter_session_affinity(Affinity const &session_affinity)
{
	Affinity::Space    const &child_space    = _resources.affinity.space();
	Affinity::Location const &child_location = _resources.affinity.location();

	/* check if no valid affinity space was specified */
	if (session_affinity.space().total() == 0)
		return Affinity(child_space, child_location);

	Affinity::Space    const &session_space    = session_affinity.space();
	Affinity::Location const &session_location = session_affinity.location();

	/* scale resolution of resulting space */
	Affinity::Space space(child_space.multiply(session_space));
	Affinity::Location child_session(child_location.xpos(), child_location.ypos(),
	                                 child_location.width() * session_location.width(),
	                                 child_location.height() * session_location.height());

	/* subordinate session affinity to child affinity subspace */
	Affinity::Location location(child_session
	                            .multiply_position(session_space)
	                            .transpose(session_location.xpos() * child_space.width(),
	                                       session_location.ypos() * child_space.height()));

	return Affinity(space, location);
}


void Sandbox::Child::announce_service(Service::Name const &service_name)
{
	if (_verbose.enabled())
		log("child \"", name(), "\" announces service \"", service_name, "\"");

	bool found = false;
	_child_services.for_each([&] (Routed_service &service) {
		if (service.has_id_space(_session_requester.id_space())
		 && service.name() == service_name)
			found = true; });

	if (!found)
		error(name(), ": illegal announcement of "
		      "service \"", service_name, "\"");
}


void Sandbox::Child::resource_request(Parent::Resource_args const &args)
{
	log("child \"", name(), "\" requests resources: ", args);

	_requested_resources.construct(args);
	_report_update_trigger.trigger_immediate_report_update();
}


Sandbox::Child::Child(Env                      &env,
                      Allocator                &alloc,
                      Verbose            const &verbose,
                      Id                        id,
                      Report_update_trigger    &report_update_trigger,
                      Xml_node                  start_node,
                      Default_route_accessor   &default_route_accessor,
                      Default_caps_accessor    &default_caps_accessor,
                      Name_registry            &name_registry,
                      Ram_quota                 ram_limit,
                      Cap_quota                 cap_limit,
                      Ram_limit_accessor       &ram_limit_accessor,
                      Cap_limit_accessor       &cap_limit_accessor,
                      Prio_levels               prio_levels,
                      Affinity::Space const    &affinity_space,
                      Registry<Parent_service> &parent_services,
                      Registry<Routed_service> &child_services,
                      Registry<Local_service>  &local_services)
:
	_env(env), _alloc(alloc), _verbose(verbose), _id(id),
	_report_update_trigger(report_update_trigger),
	_list_element(this),
	_start_node(_alloc, start_node),
	_default_route_accessor(default_route_accessor),
	_default_caps_accessor(default_caps_accessor),
	_ram_limit_accessor(ram_limit_accessor),
	_cap_limit_accessor(cap_limit_accessor),
	_name_registry(name_registry),
	_heartbeat_enabled(start_node.has_sub_node("heartbeat")),
	_resources(_resources_from_start_node(start_node, prio_levels, affinity_space,
	                                      default_caps_accessor.default_caps(), cap_limit)),
	_resources_clamped_to_limit((_clamp_resources(ram_limit, cap_limit), true)),
	_parent_services(parent_services),
	_child_services(child_services),
	_local_services(local_services),
	_session_requester(_env.ep().rpc_ep(), _env.ram(), _env.rm())
{
	if (_verbose.enabled()) {
		log("child \"",       _unique_name, "\"");
		log("  RAM quota:  ", _resources.effective_ram_quota());
		log("  cap quota:  ", _resources.effective_cap_quota());
		log("  ELF binary: ", _binary_name);
		log("  priority:   ", _resources.priority);
	}

	/*
	 * Determine services provided by the child
	 */
	_provides_sub_node(start_node)
		.for_each_sub_node("service",
		                   [&] (Xml_node node) { _add_service(node); });

	/*
	 * Construct inline config ROM service if "config" node is present.
	 */
	if (start_node.has_sub_node("config"))
		_config_rom_service.construct(*this);
}


Sandbox::Child::~Child() { }
