/*
 * \brief  Internal model of the XML configuration
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


struct Config_model::Node : Noncopyable, Interface, private List_model<Node>::Element
{
	friend class List_model<Node>;
	friend class List<Node>;

	static bool type_matches(Xml_node const &xml);

	virtual bool matches(Xml_node const &) const = 0;

	virtual void update(Xml_node const &) = 0;

	virtual void apply_child_restart(Xml_node const &) { /* only implemented by 'Start_node' */ }

	virtual void trigger_start_child() { /* only implemented by 'Start_node' */ }
};


struct Config_model::Parent_provides_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("parent-provides");
	}

	Parent_provides_model _model;

	Parent_provides_node(Allocator &alloc, Verbose const &verbose,
	                     Parent_provides_model::Factory &factory)
	:
		_model(alloc, verbose, factory)
	{ }

	bool matches(Xml_node const &xml) const override { return type_matches(xml); }

	void update(Xml_node const &xml) override { _model.update_from_xml(xml); }
};


struct Config_model::Default_route_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("default-route");
	}

	Allocator                   &_alloc;
	Constructible<Buffered_xml> &_default_route;

	Default_route_node(Allocator &alloc, Constructible<Buffered_xml> &default_route)
	: _alloc(alloc), _default_route(default_route) { }

	~Default_route_node() { _default_route.destruct(); }

	bool matches(Xml_node const &xml) const override { return type_matches(xml); }

	void update(Xml_node const &xml) override
	{
		if (!_default_route.constructed() || _default_route->xml().differs_from(xml))
			_default_route.construct(_alloc, xml);
	}
};


struct Config_model::Default_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("default");
	}

	Cap_quota &_default_caps;

	Default_node(Cap_quota &default_caps) : _default_caps(default_caps) { }

	bool matches(Xml_node const &xml) const override { return type_matches(xml); }

	void update(Xml_node const &xml) override
	{
		_default_caps = Cap_quota { xml.attribute_value("caps", 0UL) };
	}
};


struct Config_model::Affinity_space_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("affinity-space");
	}

	Constructible<Affinity::Space> &_affinity_space;

	Affinity_space_node(Constructible<Affinity::Space> &affinity_space)
	: _affinity_space(affinity_space) { }

	~Affinity_space_node() { _affinity_space.destruct(); }

	bool matches(Xml_node const &xml) const override { return type_matches(xml); }

	void update(Xml_node const &xml) override
	{
		_affinity_space.construct(xml.attribute_value("width",  1u),
		                          xml.attribute_value("height", 1u));
	}
};


struct Config_model::Start_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("start") || xml.has_type("alias");
	}

	Start_model _model;

	/*
	 * \throw Start_model::Factory::Creation_failed
	 */
	Start_node(Start_model::Factory &factory, Xml_node const &xml)
	: _model(factory, xml) { }

	bool matches(Xml_node const &xml) const override
	{
		return type_matches(xml) && _model.matches(xml);
	}

	void update(Xml_node const &xml) override
	{
		_model.update_from_xml(xml);
	}

	void apply_child_restart(Xml_node const &xml) override
	{
		_model.apply_child_restart(xml);
	}

	void trigger_start_child() override
	{
		_model.trigger_start_child();
	}
};


struct Config_model::Report_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("report");
	}

	Version  const &_version;
	State_reporter &_state_reporter;

	Report_node(Version const &version, State_reporter &state_reporter)
	: _version(version), _state_reporter(state_reporter) { }

	~Report_node()
	{
		_state_reporter.apply_config(_version, Xml_node("<empty/>"));
	}

	bool matches(Xml_node const &xml) const override { return type_matches(xml); }

	void update(Xml_node const &xml) override
	{
		_state_reporter.apply_config(_version, xml);
	}
};


struct Config_model::Resource_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("resource");
	}

	enum class Category { RAM, CAP } const _category;

	class Unknown_resource_name : Exception { };

	static Category _category_from_xml(Xml_node const &xml)
	{
		typedef String<16> Name;
		Name const name = xml.attribute_value("name", Name());

		if (name == "RAM") return Category::RAM;
		if (name == "CAP") return Category::CAP;

		throw Unknown_resource_name();
	}

	Preservation &_keep;

	/*
	 * \throw Unknown_resource_name
	 */
	Resource_node(Preservation &keep, Xml_node xml)
	:
		_category(_category_from_xml(xml)), _keep(keep)
	{ }

	~Resource_node()
	{
		switch (_category) {
		case Category::RAM: _keep.ram  = Preservation::default_ram();  break;
		case Category::CAP: _keep.caps = Preservation::default_caps(); break;
		}
	}

	bool matches(Xml_node const &xml) const override
	{
		return type_matches(xml) && _category == _category_from_xml(xml);
	}

	void update(Xml_node const &xml) override
	{
		switch (_category) {

		case Category::RAM:
			{
				Number_of_bytes keep { Preservation::default_ram().value };
				_keep.ram = { xml.attribute_value("preserve", keep) };
				break;
			}

		case Category::CAP:
			{
				size_t keep = Preservation::default_caps().value;
				_keep.caps = { xml.attribute_value("preserve", keep) };
				break;
			}
		}
	}
};


struct Config_model::Heartbeat_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("heartbeat");
	}

	Heartbeat &_heartbeat;

	Heartbeat_node(Heartbeat &heartbeat) : _heartbeat(heartbeat) { }

	~Heartbeat_node()
	{
		_heartbeat.disable();
	}

	bool matches(Xml_node const &xml) const override { return type_matches(xml); }

	void update(Xml_node const &xml) override
	{
		_heartbeat.apply_config(xml);
	}
};


struct Config_model::Service_node : Node
{
	static bool type_matches(Xml_node const &xml)
	{
		return xml.has_type("service");
	}

	Service_model::Factory &_factory;

	Service_model &_model;

	Service_node(Service_model::Factory &factory, Xml_node const &xml)
	: _factory(factory), _model(factory.create_service(xml)) { }

	~Service_node() { _factory.destroy_service(_model); }

	bool matches(Xml_node const &xml) const override
	{
		return type_matches(xml) && _model.matches(xml);
	}

	void update(Xml_node const &xml) override { _model.update_from_xml(xml); }
};


bool Config_model::Node::type_matches(Xml_node const &xml)
{
	return Parent_provides_node::type_matches(xml)
	    || Default_route_node  ::type_matches(xml)
	    || Default_node        ::type_matches(xml)
	    || Start_node          ::type_matches(xml)
	    || Affinity_space_node ::type_matches(xml)
	    || Report_node         ::type_matches(xml)
	    || Resource_node       ::type_matches(xml)
	    || Heartbeat_node      ::type_matches(xml)
	    || Service_node        ::type_matches(xml);
}


void Config_model::update_from_xml(Xml_node                 const &xml,
                                   Allocator                      &alloc,
                                   Reconstructible<Verbose>       &verbose,
                                   Version                        &version,
                                   Preservation                   &preservation,
                                   Constructible<Buffered_xml>    &default_route,
                                   Cap_quota                      &default_caps,
                                   Prio_levels                    &prio_levels,
                                   Constructible<Affinity::Space> &affinity_space,
                                   Start_model::Factory           &child_factory,
                                   Parent_provides_model::Factory &parent_service_factory,
                                   Service_model::Factory         &service_factory,
                                   State_reporter                 &state_reporter,
                                   Heartbeat                      &heartbeat)
{
	/* config version to be reflected in state reports */
	version = xml.attribute_value("version", Version());

	preservation.reset();

	prio_levels = ::Sandbox::prio_levels_from_xml(xml);

	affinity_space.destruct();

	verbose.construct(xml);

	class Unknown_element_type : Exception { };

	auto destroy = [&] (Node &node) { Genode::destroy(alloc, &node); };

	auto create = [&] (Xml_node const &xml) -> Node &
	{
		if (Parent_provides_node::type_matches(xml))
			return *new (alloc)
				Parent_provides_node(alloc, *verbose, parent_service_factory);

		if (Default_route_node::type_matches(xml))
			return *new (alloc) Default_route_node(alloc, default_route);

		if (Default_node::type_matches(xml))
			return *new (alloc) Default_node(default_caps);

		if (Start_node::type_matches(xml))
			return *new (alloc) Start_node(child_factory, xml);

		if (Affinity_space_node::type_matches(xml))
			return *new (alloc) Affinity_space_node(affinity_space);

		if (Report_node::type_matches(xml))
			return *new (alloc) Report_node(version, state_reporter);

		if (Resource_node::type_matches(xml))
			return *new (alloc) Resource_node(preservation, xml);

		if (Heartbeat_node::type_matches(xml))
			return *new (alloc) Heartbeat_node(heartbeat);

		if (Service_node::type_matches(xml))
			return *new (alloc) Service_node(service_factory, xml);

		error("unknown config element type <", xml.type(), ">");
		throw Unknown_element_type();
	};

	auto update = [&] (Node &node, Xml_node const &xml) { node.update(xml); };

	try {
		update_list_model_from_xml(_model, xml, create, destroy, update);
	}
	catch (Unknown_element_type) {
		error("unable to apply complete configuration"); }
	catch (Start_model::Factory::Creation_failed) {
		error("child creation failed"); }
}


void Config_model::apply_children_restart(Xml_node const &xml)
{
	class Unexpected : Exception { };
	auto destroy = [&] (Node &) { };
	auto create  = [&] (Xml_node const &) -> Node & { throw Unexpected(); };
	auto update  = [&] (Node &node, Xml_node const &xml)
	{
		node.apply_child_restart(xml);
	};

	try {
		update_list_model_from_xml(_model, xml, create, destroy, update);
	}
	catch (...) { };
}


void Config_model::trigger_start_children()
{
	_model.for_each([&] (Node &node) {
		node.trigger_start_child(); });
}
