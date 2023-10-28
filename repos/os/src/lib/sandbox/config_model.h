/*
 * \brief  Internal model of the XML configuration
 * \author Norman Feske
 * \date   2021-04-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONFIG_MODEL_H_
#define _CONFIG_MODEL_H_

/* Genode includes */
#include <util/list_model.h>

/* local includes */
#include <heartbeat.h>

namespace Sandbox {

	struct Parent_provides_model;
	struct Start_model;
	struct Service_model;
	struct Config_model;
}


struct Sandbox::Parent_provides_model : Noncopyable
{
	struct Factory : Interface, Noncopyable
	{
		virtual Parent_service &create_parent_service(Service::Name const &) = 0;
	};

	Allocator     &_alloc;
	Verbose const &_verbose;
	Factory       &_factory;

	struct Node : Noncopyable, List_model<Node>::Element
	{
		Parent_service &service;

		Node(Factory &factory, Service::Name const &name)
		:
			service(factory.create_parent_service(name))
		{ }

		~Node()
		{
			/*
			 * The destruction of the 'Parent_service' is deferred to the
			 * handling of abandoned entries of the 'Registry<Parent_service>'.
			 */
			service.abandon();
		}

		static bool type_matches(Xml_node const &) { return true; }

		bool matches(Xml_node const &xml) const
		{
			return xml.attribute_value("name", Service::Name()) == service.name();
		};
	};

	List_model<Node> _model { };

	Parent_provides_model(Allocator &alloc, Verbose const &verbose, Factory &factory)
	:
		_alloc(alloc), _verbose(verbose), _factory(factory)
	{ }

	~Parent_provides_model()
	{
		update_from_xml(Xml_node("<empty/>"));
	}

	void update_from_xml(Xml_node const &xml)
	{
		bool first_log = true;

		auto create = [&] (Xml_node const &xml) -> Node &
		{
			Service::Name const name = xml.attribute_value("name", Service::Name());

			if (_verbose.enabled()) {
				if (first_log)
					log("parent provides");

				log("  service \"", name, "\"");
				first_log = false;
			}

			return *new (_alloc) Node(_factory, name);
		};

		auto destroy = [&] (Node &node) { Genode::destroy(_alloc, &node); };

		auto update = [&] (Node &, Xml_node const &) { };

		try {
			_model.update_from_xml(xml, create, destroy, update);
		} catch (...) {
			error("unable to apply complete configuration");
		}
	}
};


struct Sandbox::Start_model : Noncopyable
{
	/*
	 * The 'Start_model' represents both '<alias>' nodes and '<start>' nodes
	 * because both node types share the same name space.
	 */

	typedef Child_policy::Name Name;
	typedef Child::Version     Version;

	static char const *start_type() { return "start"; }
	static char const *alias_type() { return "alias"; }

	struct Factory : Interface
	{
		class Creation_failed : Exception { };

		virtual bool ready_to_create_child(Name const &, Version const &) const = 0;

		/*
		 * \throw Creation_failed
		 */
		virtual Child &create_child(Xml_node const &start) = 0;

		virtual void update_child(Child &child, Xml_node const &start) = 0;

		/*
		 * \throw Creation_failed
		 */
		virtual Alias &create_alias(Name const &) = 0;

		virtual void destroy_alias(Alias &) = 0;
	};

	Name    const _name;
	Version const _version;

	Factory &_factory;

	bool _alias = false;

	struct { Child *_child_ptr = nullptr; };
	struct { Alias *_alias_ptr = nullptr; };

	void _reset()
	{
		if (_child_ptr) _child_ptr->abandon();
		if (_alias_ptr) _factory.destroy_alias(*_alias_ptr);

		_child_ptr = nullptr;
		_alias_ptr = nullptr;
	}

	bool matches(Xml_node const &xml) const
	{
		return _name    == xml.attribute_value("name",    Name())
		    && _version == xml.attribute_value("version", Version());
	}

	Start_model(Factory &factory, Xml_node const &xml)
	:
		_name(xml.attribute_value("name", Name())),
		_version(xml.attribute_value("version", Version())),
		_factory(factory)
	{ }

	~Start_model() { _reset(); }

	void update_from_xml(Xml_node const &xml)
	{
		/* handle case where the node keeps the name but changes the type */

		bool const orig_alias = _alias;
		_alias = xml.has_type("alias");
		if (orig_alias != _alias)
			_reset();

		/* create alias or child depending of the node type */

		if (_alias) {

			if (!_alias_ptr)
				_alias_ptr = &_factory.create_alias(_name);

		} else {

			if (!_child_ptr && _factory.ready_to_create_child(_name, _version))
				_child_ptr = &_factory.create_child(xml);
		}

		/* update */

		if (_alias_ptr)
			_alias_ptr->update(xml);

		if (_child_ptr)
			_factory.update_child(*_child_ptr, xml);
	}

	void apply_child_restart(Xml_node const &xml)
	{
		if (_child_ptr && _child_ptr->restart_scheduled()) {

			/* tear down */
			_child_ptr->abandon();
			_child_ptr = nullptr;

			/* respawn */
			update_from_xml(xml);
		}
	}

	void trigger_start_child()
	{
		if (_child_ptr)
			_child_ptr->try_start();
	}
};


struct Sandbox::Service_model : Interface, Noncopyable
{
	struct Factory : Interface, Noncopyable
	{
		virtual Service_model &create_service(Xml_node const &) = 0;
		virtual void destroy_service(Service_model &) = 0;
	};

	virtual void update_from_xml(Xml_node const &) = 0;

	virtual bool matches(Xml_node const &) = 0;
};


class Sandbox::Config_model : Noncopyable
{
	private:

		struct Node;

		List_model<Node> _model { };

		struct Parent_provides_node;
		struct Default_route_node;
		struct Default_node;
		struct Affinity_space_node;
		struct Start_node;
		struct Report_node;
		struct Resource_node;
		struct Heartbeat_node;
		struct Service_node;

	public:

		typedef State_reporter::Version Version;

		void update_from_xml(Xml_node                 const &,
		                     Allocator                      &,
		                     Reconstructible<Verbose>       &,
		                     Version                        &,
		                     Preservation                   &,
		                     Constructible<Buffered_xml>    &,
		                     Cap_quota                      &,
		                     Prio_levels                    &,
		                     Constructible<Affinity::Space> &,
		                     Start_model::Factory           &,
		                     Parent_provides_model::Factory &,
		                     Service_model::Factory         &,
		                     State_reporter                 &,
		                     Heartbeat                      &);

		void apply_children_restart(Xml_node const &);

		/*
		 * Call 'Child::try_start' for each child in start-node order
		 */
		void trigger_start_children();
};

#endif /* _CONFIG_MODEL_H_ */
