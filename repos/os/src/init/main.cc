/*
 * \brief  Init component
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		template <typename FN>
		void for_each_child(FN const &fn) const
		{
			Genode::List_element<Child> const *curr = first();
			for (; curr; curr = curr->next())
				fn(*curr->object());
		}

		template <typename FN>
		void for_each_child(FN const &fn)
		{
			Genode::List_element<Child> *curr = first(), *next = nullptr;
			for (; curr; curr = next) {
				next = curr->next();
				fn(*curr->object());
			}
		}

		void report_state(Xml_generator &xml, Report_detail const &detail) const
		{
			for_each_child([&] (Child &child) { child.report_state(xml, detail); });

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


struct Init::Main : State_reporter::Producer, Child::Default_route_accessor,
                    Child::Ram_limit_accessor
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

	static Ram_quota _preserved_ram_from_config(Xml_node config)
	{
		Number_of_bytes preserve { 40*sizeof(long)*1024 };

		config.for_each_sub_node("resource", [&] (Xml_node node) {
			if (node.attribute_value("name", String<16>()) == "RAM")
				preserve = node.attribute_value("preserve", preserve); });

		return Ram_quota { preserve };
	}

	Ram_quota _ram_limit { 0 };

	/**
	 * Child::Ram_limit_accessor interface
	 */
	Ram_quota ram_limit() override { return _ram_limit; }

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

	void _update_aliases_from_config();
	void _update_parent_services_from_config();
	void _abandon_obsolete_children();
	void _update_children_config();
	void _destroy_abandoned_parent_services();
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


void Init::Main::_update_parent_services_from_config()
{
	Xml_node const node = _config.xml().has_sub_node("parent-provides")
	                    ? _config.xml().sub_node("parent-provides")
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

	if (_verbose->enabled())
		log("parent provides");

	/* register new services */
	node.for_each_sub_node("service", [&] (Xml_node service) {

		Service::Name const name = service.attribute_value("name", Service::Name());

		bool registered = false;
		_parent_services.for_each([&] (Parent_service const &service) {
			if (service.name() == name)
				registered = true; });

		if (!registered) {
			new (_heap) Init::Parent_service(_parent_services, _env, name);
			if (_verbose->enabled())
				log("  service \"", name, "\"");
		}
	});
}


void Init::Main::_destroy_abandoned_parent_services()
{
	_parent_services.for_each([&] (Parent_service &service) {
		if (service.abandoned())
			destroy(_heap, &service); });
}


void Init::Main::_update_aliases_from_config()
{
	/* remove all known aliases */
	while (_children.any_alias()) {
		Init::Alias *alias = _children.any_alias();
		_children.remove_alias(alias);
		destroy(_heap, alias);
	}

	/* create aliases */
	_config.xml().for_each_sub_node("alias", [&] (Xml_node alias_node) {

		try {
			_children.insert_alias(new (_heap) Alias(alias_node)); }
		catch (Alias::Name_is_missing) {
			warning("missing 'name' attribute in '<alias>' entry"); }
		catch (Alias::Child_is_missing) {
			warning("missing 'child' attribute in '<alias>' entry"); }
	});
}


void Init::Main::_abandon_obsolete_children()
{
	_children.for_each_child([&] (Child &child) {

		Child_policy::Name const name = child.name();

		bool obsolete = true;
		_config.xml().for_each_sub_node("start", [&] (Xml_node node) {
			if (node.attribute_value("name", Child_policy::Name()) == name)
				obsolete = false; });

		if (obsolete)
			child.abandon();
	});
}


void Init::Main::_update_children_config()
{
	for (;;) {

		/*
		 * Children are abandoned if any of their client sessions can no longer
		 * be routed or result in a different route. As each child may be a
		 * service, an avalanche effect may occur. It stops if no update causes
		 * a potential side effect in one iteration over all chilren.
		 */
		bool side_effects = false;

		_config.xml().for_each_sub_node("start", [&] (Xml_node node) {

			Child_policy::Name const start_node_name =
				node.attribute_value("name", Child_policy::Name());

			_children.for_each_child([&] (Child &child) {
				if (child.name() == start_node_name) {
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


void Init::Main::_handle_config()
{
	_config.update();

	_verbose.construct(_config.xml());
	_state_reporter.apply_config(_config.xml());

	/* determine default route for resolving service requests */
	try {
		_default_route.construct(_heap, _config.xml().sub_node("default-route")); }
	catch (...) { }

	_update_aliases_from_config();
	_update_parent_services_from_config();
	_abandon_obsolete_children();
	_update_children_config();

	/* kill abandoned children */
	_children.for_each_child([&] (Child &child) {
		if (child.abandoned()) {
			_children.remove(&child);
			destroy(_heap, &child);
		}
	});

	_destroy_abandoned_parent_services();

	Ram_quota const preserved_ram = _preserved_ram_from_config(_config.xml());

	Ram_quota avail_ram { _env.ram().avail() };

	if (preserved_ram.value > avail_ram.value) {
		error("RAM preservation exceeds available memory");
		return;
	}

	/* deduce preserved quota from available quota */
	avail_ram = Ram_quota { avail_ram.value - preserved_ram.value };

	/* initial RAM limit before starting new children */
	_ram_limit = Ram_quota { avail_ram.value };

	/* variable used to track the RAM taken by new started children */
	Ram_quota used_ram { 0 };

	/* create new children */
	try {
		_config.xml().for_each_sub_node("start", [&] (Xml_node start_node) {

			/* skip start node if corresponding child already exists */
			bool exists = false;
			_children.for_each_child([&] (Child const &child) {
				if (child.name() == start_node.attribute_value("name", Child_policy::Name()))
					exists = true; });
			if (exists) {
				return;
			}

			if (used_ram.value > avail_ram.value) {
				error("RAM exhausted while starting childen");
				throw Ram_session::Alloc_failed();
			}

			try {
				Init::Child &child = *new (_heap)
					Init::Child(_env, _heap, *_verbose,
					            Init::Child::Id { ++_child_cnt }, _state_reporter,
					            start_node, *this, _children, *this,
					            read_prio_levels(_config.xml()),
					            read_affinity_space(_config.xml()),
					            _parent_services, _child_services);
				_children.insert(&child);

				/* account for the start XML node buffered in the child */
				size_t const metadata_overhead = start_node.size()
				                               + sizeof(Init::Child);
				/* track used memory and RAM limit */
				used_ram = Ram_quota { used_ram.value
				                     + child.ram_quota().value
				                     + metadata_overhead };
				_ram_limit = Ram_quota { avail_ram.value - used_ram.value };
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

	/*
	 * Initiate RAM sessions of all new children
	 */
	_children.for_each_child([&] (Child &child) {
		child.initiate_env_ram_session(); });

	/*
	 * Initiate remaining environment sessions of all new children
	 */
	_children.for_each_child([&] (Child &child) {
		child.initiate_env_sessions(); });
}


void Component::construct(Genode::Env &env) { static Init::Main main(env); }

