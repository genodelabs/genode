/*
 * \brief  Init component
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* init includes */
#include <init/child.h>

namespace Init {

	using namespace Genode;

	/**
	 * Read priority-levels declaration from config
	 */
	inline long read_prio_levels(Xml_node config)
	{
		long const prio_levels = config.attribute_value("prio_levels", 0UL);

		if (prio_levels && (prio_levels != (1 << log2(prio_levels)))) {
			warning("prio levels is not power of two, priorities are disabled");
			return 0;
		}
		return prio_levels;
	}


	/**
	 * Read affinity-space parameters from config
	 *
	 * If no affinity space is declared, construct a space with a single element,
	 * width and height being 1. If only one of both dimensions is specified, the
	 * other dimension is set to 1.
	 */
	inline Genode::Affinity::Space read_affinity_space(Xml_node config)
	{
		try {
			Xml_node node = config.sub_node("affinity-space");
			return Affinity::Space(node.attribute_value<unsigned long>("width",  1),
			                       node.attribute_value<unsigned long>("height", 1));
		} catch (...) {
			return Affinity::Space(1, 1); }
	}


	/**
	 * Read parent-provided services from config
	 */
	inline void determine_parent_services(Registry<Init::Parent_service> &services,
	                                      Xml_node config, Allocator &alloc,
	                                      bool verbose)
	{
		if (verbose)
			log("parent provides");

		config.sub_node("parent-provides")
		      .for_each_sub_node("service", [&] (Xml_node node) {

			Service::Name name = node.attribute_value("name", Service::Name());

			new (alloc) Init::Parent_service(services, name);
			if (verbose)
				log("  service \"", name, "\"");
		});
	}
}


/********************
 ** Child registry **
 ********************/

namespace Init { struct Alias; }


/**
 * Representation of an alias for a child
 */
struct Init::Alias : Genode::List<Alias>::Element
{
	typedef Genode::String<128> Name;
	typedef Genode::String<128> Child;

	Name  name;
	Child child;

	/**
	 * Exception types
	 */
	class Name_is_missing  { };
	class Child_is_missing { };

	/**
	 * Utility to read a string attribute from an XML node
	 *
	 * \param STR        string type
	 * \param EXC        exception type raised if attribute is not present
	 *
	 * \param node       XML node
	 * \param attr_name  name of attribute to read
	 */
	template <typename STR, typename EXC>
	static STR _read_string_attr(Genode::Xml_node node, char const *attr_name)
	{
		char buf[STR::size()];

		if (!node.has_attribute(attr_name))
			throw EXC();

		node.attribute(attr_name).value(buf, sizeof(buf));

		return STR(buf);
	}

	/**
	 * Constructor
	 *
	 * \throw Name_is_missing
	 * \throw Child_is_missing
	 */
	Alias(Genode::Xml_node alias)
	:
		name (_read_string_attr<Name, Name_is_missing> (alias, "name")),
		child(_read_string_attr<Name, Child_is_missing>(alias, "child"))
	{ }
};


namespace Init {

	typedef Genode::List<Genode::List_element<Child> > Child_list;

	struct Child_registry;
}


class Init::Child_registry : public Name_registry, Child_list
{
	private:

		List<Alias> _aliases;

	public:

		/**
		 * Exception type
		 */
		class Alias_name_is_not_unique { };

		/**
		 * Register child
		 */
		void insert(Child *child)
		{
			Child_list::insert(&child->_list_element);
		}

		/**
		 * Unregister child
		 */
		void remove(Child *child)
		{
			Child_list::remove(&child->_list_element);
		}

		/**
		 * Register alias
		 */
		void insert_alias(Alias *alias)
		{
			if (!unique(alias->name.string())) {
				error("alias name ", alias->name, " is not unique");
				throw Alias_name_is_not_unique();
			}
			_aliases.insert(alias);
		}

		/**
		 * Unregister alias
		 */
		void remove_alias(Alias *alias)
		{
			_aliases.remove(alias);
		}

		/**
		 * Return any of the registered children, or 0 if no child exists
		 */
		Child *any()
		{
			return first() ? first()->object() : 0;
		}

		/**
		 * Return any of the registered aliases, or 0 if no alias exists
		 */
		Alias *any_alias()
		{
			return _aliases.first() ? _aliases.first() : 0;
		}

		void report_state(Xml_generator &xml, Report_detail const &detail) const
		{
			Genode::List_element<Child> const *curr = first();
			for (; curr; curr = curr->next())
				curr->object()->report_state(xml, detail);

			/* check for name clash with an existing alias */
			for (Alias const *a = _aliases.first(); a; a = a->next()) {
				xml.node("alias", [&] () {
					xml.attribute("name", a->name);
					xml.attribute("child", a->child);
				});
			}

		}


		/*****************************
		 ** Name-registry interface **
		 *****************************/

		bool unique(const char *name) const
		{
			/* check for name clash with an existing child */
			Genode::List_element<Child> const *curr = first();
			for (; curr; curr = curr->next())
				if (curr->object()->has_name(name))
					return false;

			/* check for name clash with an existing alias */
			for (Alias const *a = _aliases.first(); a; a = a->next()) {
				if (Alias::Name(name) == a->name)
					return false;
			}

			return true;
		}

		Name deref_alias(Name const &name) override
		{
			for (Alias const *a = _aliases.first(); a; a = a->next())
				if (name == a->name)
					return a->child;

			return name;
		}
};


namespace Init {
	struct State_reporter;
	struct Main;
}


class Init::State_reporter : public Report_update_trigger
{
	public:

		struct Producer
		{
			virtual void produce_state_report(Xml_generator &xml,
			                                  Report_detail const &) const = 0;
		};

	private:

		Env &_env;

		Producer &_producer;

		Constructible<Reporter> _reporter;

		size_t _buffer_size = 0;

		Reconstructible<Report_detail> _report_detail;

		unsigned _report_delay_ms = 0;

		/* version string from config, to be reflected in the report */
		typedef String<64> Version;
		Version _version;

		Constructible<Timer::Connection> _timer;

		Signal_handler<State_reporter> _timer_handler {
			_env.ep(), *this, &State_reporter::_handle_timer };

		bool _scheduled = false;

		void _handle_timer()
		{
			_scheduled = false;

			try {
				Reporter::Xml_generator xml(*_reporter, [&] () {

					if (_version.valid())
						xml.attribute("version", _version);

					_producer.produce_state_report(xml, *_report_detail);
				});
			}
			catch(Xml_generator::Buffer_exceeded) {

				error("state report exceeds maximum size");

				/* try to reflect the error condition as state report */
				try {
					Reporter::Xml_generator xml(*_reporter, [&] () {
						xml.attribute("error", "report buffer exceeded"); });
				}
				catch (...) { }
			}
		}

	public:

		State_reporter(Env &env, Producer &producer)
		:
			_env(env), _producer(producer)
		{ }

		void apply_config(Xml_node config)
		{
			try {
				Xml_node report = config.sub_node("report");

				/* (re-)construct reporter whenever the buffer size is changed */
				Number_of_bytes const buffer_size =
					report.attribute_value("buffer", Number_of_bytes(4096));

				if (buffer_size != _buffer_size || !_reporter.constructed()) {
					_buffer_size = buffer_size;
					_reporter.construct(_env, "state", "state", _buffer_size);
				}

				_report_detail.construct(report);
				_report_delay_ms = report.attribute_value("delay_ms", 100UL);
				_reporter->enabled(true);
			}
			catch (Xml_node::Nonexistent_sub_node) {
				_report_detail.construct();
				_report_delay_ms = 0;
				if (_reporter.constructed())
					_reporter->enabled(false);
			}

			_version = config.attribute_value("version", Version());

			if (_report_delay_ms) {

				if (!_timer.constructed()) {
					_timer.construct(_env);
					_timer->sigh(_timer_handler);
				}

				trigger_report_update();
			}
		}

		void trigger_report_update() override
		{
			if (!_scheduled && _timer.constructed() && _report_delay_ms) {
				_timer->trigger_once(_report_delay_ms*1000);
				_scheduled = true;
			}
		}
};


struct Init::Main : State_reporter::Producer, Child::Default_route_accessor
{
	Env &_env;

	Registry<Init::Parent_service> _parent_services;
	Registry<Routed_service>       _child_services;
	Child_registry                 _children;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Reconstructible<Verbose> _verbose { _config.xml() };

	Constructible<Buffered_xml> _default_route;

	unsigned _child_cnt = 0;

	void _handle_resource_avail() { }

	void produce_state_report(Xml_generator &xml, Report_detail const &detail) const
	{
		if (detail.init_ram())
			xml.node("ram", [&] () { generate_ram_info(xml, _env.ram()); });

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

	State_reporter _state_reporter { _env, *this };

	Signal_handler<Main> _resource_avail_handler {
		_env.ep(), *this, &Main::_handle_resource_avail };

	void _handle_config();

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);

		/* prevent init to block for resource upgrades (never satisfied by core) */
		_env.parent().resource_avail_sigh(_resource_avail_handler);

		_handle_config();
	}
};


void Init::Main::_handle_config()
{
	/* kill all currently running children */
	while (_children.any()) {
		Init::Child *child = _children.any();
		_children.remove(child);
		destroy(_heap, child);
	}

	/* remove all known aliases */
	while (_children.any_alias()) {
		Init::Alias *alias = _children.any_alias();
		_children.remove_alias(alias);
		destroy(_heap, alias);
	}

	/* reset knowledge about parent services */
	_parent_services.for_each([&] (Init::Parent_service &service) {
		destroy(_heap, &service); });

	_config.update();

	_verbose.construct(_config.xml());
	_state_reporter.apply_config(_config.xml());

	try { determine_parent_services(_parent_services, _config.xml(),
	                                _heap, _verbose->enabled()); }
	catch (...) { }

	/* determine default route for resolving service requests */
	try {
		_default_route.construct(_heap, _config.xml().sub_node("default-route")); }
	catch (...) { }

	/* create aliases */
	_config.xml().for_each_sub_node("alias", [&] (Xml_node alias_node) {

		try {
			_children.insert_alias(new (_heap) Alias(alias_node)); }
		catch (Alias::Name_is_missing) {
			warning("missing 'name' attribute in '<alias>' entry"); }
		catch (Alias::Child_is_missing) {
			warning("missing 'child' attribute in '<alias>' entry"); }
	});

	/* create children */
	try {
		_config.xml().for_each_sub_node("start", [&] (Xml_node start_node) {

			try {
				_children.insert(new (_heap)
				                 Init::Child(_env, _heap, *_verbose,
				                             Init::Child::Id { ++_child_cnt },
				                             _state_reporter,
				                             start_node, *this,
				                             _children, read_prio_levels(_config.xml()),
				                             read_affinity_space(_config.xml()),
				                             _parent_services, _child_services));
			}
			catch (Rom_connection::Rom_connection_failed) {
				/*
				 * The binary does not exist. An error message is printed
				 * by the Rom_connection constructor.
				 */
			}
			catch (Allocator::Out_of_memory) {
				warning("local memory exhausted during child creation"); }
			catch (Ram_session::Alloc_failed) {
				warning("failed to allocate memory during child construction"); }
			catch (Child::Missing_name_attribute) {
				warning("skipped startup of nameless child"); }
			catch (Region_map::Attach_failed) {
				warning("failed to attach dataspace to local address space "
				        "during child construction"); }
			catch (Parent::Service_denied) {
				warning("failed to create session during child construction"); }
		});
	}
	catch (Xml_node::Nonexistent_sub_node) { error("no children to start"); }
	catch (Xml_node::Invalid_syntax) { error("config has invalid syntax"); }
	catch (Init::Child::Child_name_is_not_unique) { }
	catch (Init::Child_registry::Alias_name_is_not_unique) { }
}


void Component::construct(Genode::Env &env) { static Init::Main main(env); }

