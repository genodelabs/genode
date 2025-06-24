/*
 * \brief  Internal model of the configuration
 * \author Norman Feske
 * \date   2021-04-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <config_model.h>

using namespace Sandbox;


struct Config_model::Config_node : Noncopyable, Interface,
                                   private List_model<Config_node>::Element
{
	friend class List_model<Config_node>;
	friend class List<Config_node>;

	static bool type_matches(Node const &node);

	virtual bool matches(Node const &) const = 0;

	virtual void update(Node const &) = 0;

	virtual void apply_child_restart(Node const &) { /* only implemented by 'Start_node' */ }

	virtual void trigger_start_child() { /* only implemented by 'Start_node' */ }
};


struct Config_model::Parent_provides_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("parent-provides");
	}

	Parent_provides_model _model;

	Parent_provides_node(Allocator &alloc, Verbose const &verbose,
	                     Parent_provides_model::Factory &factory)
	:
		_model(alloc, verbose, factory)
	{ }

	bool matches(Node const &node) const override { return type_matches(node); }

	void update(Node const &node) override { _model.update_from_node(node); }
};


struct Config_model::Default_route_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("default-route");
	}

	Allocator                    &_alloc;
	Constructible<Buffered_node> &_default_route;

	Default_route_node(Allocator &alloc, Constructible<Buffered_node> &default_route)
	: _alloc(alloc), _default_route(default_route) { }

	~Default_route_node() { _default_route.destruct(); }

	bool matches(Node const &node) const override { return type_matches(node); }

	void update(Node const &node) override
	{
		if (!_default_route.constructed() || _default_route->differs_from(node))
			_default_route.construct(_alloc, node);
	}
};


struct Config_model::Default_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("default");
	}

	Cap_quota &_default_caps;
	Ram_quota &_default_ram;

	Default_node(Cap_quota &default_caps, Ram_quota &default_ram)
	: _default_caps(default_caps), _default_ram(default_ram) { }

	bool matches(Node const &node) const override { return type_matches(node); }

	void update(Node const &node) override
	{
		_default_caps = Cap_quota { node.attribute_value("caps", 0UL) };
		_default_ram  = Ram_quota { node.attribute_value("ram", Number_of_bytes()) };
	}
};


struct Config_model::Affinity_space_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("affinity-space");
	}

	Constructible<Affinity::Space> &_affinity_space;

	Affinity_space_node(Constructible<Affinity::Space> &affinity_space)
	: _affinity_space(affinity_space) { }

	~Affinity_space_node() { _affinity_space.destruct(); }

	bool matches(Node const &node) const override { return type_matches(node); }

	void update(Node const &node) override
	{
		_affinity_space.construct(node.attribute_value("width",  1u),
		                          node.attribute_value("height", 1u));
	}
};


struct Config_model::Start_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("start") || node.has_type("alias");
	}

	Start_model _model;

	/*
	 * \throw Start_model::Factory::Creation_failed
	 */
	Start_node(Start_model::Factory &factory, Node const &node)
	: _model(factory, node) { }

	bool matches(Node const &node) const override
	{
		return type_matches(node) && _model.matches(node);
	}

	void update(Node const &node) override
	{
		_model.update_from_node(node);
	}

	void apply_child_restart(Node const &node) override
	{
		_model.apply_child_restart(node);
	}

	void trigger_start_child() override
	{
		_model.trigger_start_child();
	}
};


struct Config_model::Report_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("report");
	}

	Version  const &_version;
	State_reporter &_state_reporter;

	Report_node(Version const &version, State_reporter &state_reporter)
	: _version(version), _state_reporter(state_reporter) { }

	~Report_node()
	{
		_state_reporter.apply_config(_version, Node());
	}

	bool matches(Node const &node) const override { return type_matches(node); }

	void update(Node const &node) override
	{
		_state_reporter.apply_config(_version, node);
	}
};


struct Config_model::Resource_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("resource");
	}

	enum class Category { RAM, CAP } const _category;

	class Unknown_resource_name : Exception { };

	static Category _category_from_node(Node const &node)
	{
		using Name = String<16>;
		Name const name = node.attribute_value("name", Name());

		if (name == "RAM") return Category::RAM;
		if (name == "CAP") return Category::CAP;

		throw Unknown_resource_name();
	}

	Preservation &_keep;

	/*
	 * \throw Unknown_resource_name
	 */
	Resource_node(Preservation &keep, Node const &node)
	:
		_category(_category_from_node(node)), _keep(keep)
	{ }

	~Resource_node()
	{
		switch (_category) {
		case Category::RAM: _keep.ram  = Preservation::default_ram();  break;
		case Category::CAP: _keep.caps = Preservation::default_caps(); break;
		}
	}

	bool matches(Node const &node) const override
	{
		return type_matches(node) && _category == _category_from_node(node);
	}

	void update(Node const &node) override
	{
		switch (_category) {

		case Category::RAM:
			{
				Number_of_bytes keep { Preservation::default_ram().value };
				_keep.ram = { node.attribute_value("preserve", keep) };
				break;
			}

		case Category::CAP:
			{
				size_t keep = Preservation::default_caps().value;
				_keep.caps = { node.attribute_value("preserve", keep) };
				break;
			}
		}
	}
};


struct Config_model::Heartbeat_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("heartbeat");
	}

	Heartbeat &_heartbeat;

	Heartbeat_node(Heartbeat &heartbeat) : _heartbeat(heartbeat) { }

	~Heartbeat_node()
	{
		_heartbeat.disable();
	}

	bool matches(Node const &node) const override { return type_matches(node); }

	void update(Node const &node) override
	{
		_heartbeat.apply_config(node);
	}
};


struct Config_model::Service_node : Config_node
{
	static bool type_matches(Node const &node)
	{
		return node.has_type("service");
	}

	Service_model::Factory &_factory;

	Service_model &_model;

	Service_node(Service_model::Factory &factory, Node const &node)
	: _factory(factory), _model(factory.create_service(node)) { }

	~Service_node() { _factory.destroy_service(_model); }

	bool matches(Node const &node) const override
	{
		return type_matches(node) && _model.matches(node);
	}

	void update(Node const &node) override { _model.update_from_node(node); }
};


bool Config_model::Config_node::type_matches(Node const &node)
{
	return Parent_provides_node::type_matches(node)
	    || Default_route_node  ::type_matches(node)
	    || Default_node        ::type_matches(node)
	    || Start_node          ::type_matches(node)
	    || Affinity_space_node ::type_matches(node)
	    || Report_node         ::type_matches(node)
	    || Resource_node       ::type_matches(node)
	    || Heartbeat_node      ::type_matches(node)
	    || Service_node        ::type_matches(node);
}


void Config_model::update_from_node(Node                     const &node,
                                    Allocator                      &alloc,
                                    Reconstructible<Verbose>       &verbose,
                                    Version                        &version,
                                    Preservation                   &preservation,
                                    Constructible<Buffered_node>   &default_route,
                                    Cap_quota                      &default_caps,
                                    Ram_quota                      &default_ram,
                                    Prio_levels                    &prio_levels,
                                    Constructible<Affinity::Space> &affinity_space,
                                    Start_model::Factory           &child_factory,
                                    Parent_provides_model::Factory &parent_service_factory,
                                    Service_model::Factory         &service_factory,
                                    State_reporter                 &state_reporter,
                                    Heartbeat                      &heartbeat)
{
	/* config version to be reflected in state reports */
	version = node.attribute_value("version", Version());

	preservation.reset();

	prio_levels = ::Sandbox::prio_levels_from_node(node);

	affinity_space.destruct();

	verbose.construct(node);

	class Unknown_element_type : Exception { };

	auto destroy = [&] (Config_node &node) { Genode::destroy(alloc, &node); };

	auto create = [&] (Node const &node) -> Config_node &
	{
		if (Parent_provides_node::type_matches(node))
			return *new (alloc)
				Parent_provides_node(alloc, *verbose, parent_service_factory);

		if (Default_route_node::type_matches(node))
			return *new (alloc) Default_route_node(alloc, default_route);

		if (Default_node::type_matches(node))
			return *new (alloc) Default_node(default_caps, default_ram);

		if (Start_node::type_matches(node))
			return *new (alloc) Start_node(child_factory, node);

		if (Affinity_space_node::type_matches(node))
			return *new (alloc) Affinity_space_node(affinity_space);

		if (Report_node::type_matches(node))
			return *new (alloc) Report_node(version, state_reporter);

		if (Resource_node::type_matches(node))
			return *new (alloc) Resource_node(preservation, node);

		if (Heartbeat_node::type_matches(node))
			return *new (alloc) Heartbeat_node(heartbeat);

		if (Service_node::type_matches(node))
			return *new (alloc) Service_node(service_factory, node);

		error("unknown config element type <", node.type(), ">");
		throw Unknown_element_type();
	};

	auto update = [&] (Config_node &config_node, Node const &node)
	{
		config_node.update(node);
	};

	try {
		_model.update_from_node(node, create, destroy, update);
	}
	catch (Unknown_element_type) {
		error("unable to apply complete configuration"); }
	catch (Start_model::Factory::Creation_failed) {
		error("child creation failed"); }
}


void Config_model::apply_children_restart(Node const &node)
{
	class Unexpected : Exception { };
	auto destroy = [&] (Config_node &) { };
	auto create  = [&] (Node const &) -> Config_node & { throw Unexpected(); };
	auto update  = [&] (Config_node &config_node, Node const &node)
	{
		config_node.apply_child_restart(node);
	};

	try {
		_model.update_from_node(node, create, destroy, update);
	}
	catch (...) { };
}


void Config_model::trigger_start_children()
{
	_model.for_each([&] (Config_node &node) {
		node.trigger_start_child(); });
}
