/*
 * \brief  Internal model of routing rules
 * \author Norman Feske
 * \date   2021-04-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ROUTE_MODEL_H_
#define _ROUTE_MODEL_H_

/* local includes */
#include <types.h>

namespace Sandbox {

	struct Checksum;
	class Route_model;
}


struct Sandbox::Checksum
{
	unsigned long value = 0;

	bool valid;

	/**
	 * Constructor
	 */
	Checksum(char const *s) : valid(s != nullptr)
	{
		if (!s)
			return;

		while (uint8_t const byte = *s++) {

			/* rotate value */
			unsigned long const sign_bit = ((long)value < 0);
			value = (value << 1) | sign_bit;

			/* xor byte to lowest 8 bit */
			value = value ^ (unsigned long)byte;
		}
	}

	template <size_t N>
	Checksum(String<N> const &s) : Checksum(s.string()) { }

	bool operator != (Checksum const &other) const
	{
		return (other.value != value) || !valid;
	}
};


class Sandbox::Route_model : Noncopyable
{
	public:

		struct Query : Noncopyable
		{
			Child_policy::Name const &child;
			Service::Name      const &service;
			Session_label      const &label;

			Checksum const service_checksum { service };
			Checksum const label_checksum   { skip_label_prefix(child.string(),
			                                                    label.string()) };

			Query(Child_policy::Name const &child,
			      Service::Name      const &service,
			      Session_label      const &label)
			:
				child(child), service(service), label(label)
			{ }
		};

		class Rule : Noncopyable, List<Rule>::Element
		{
			private:

				friend class List<Rule>;
				friend class Route_model;
				friend void destroy<Rule>(Allocator &, Rule *);

				Allocator &_alloc;

				Xml_node const _node; /* points to 'Route_model::_route_node' */

				struct Selector
				{
					typedef String<Session_label::capacity()> Label;

					enum class Type
					{
						NO_LABEL, SPECIFIC_LABEL,

						/*
						 * Presence of 'label_last', 'label_prefix',
						 * 'label_suffix', 'unscoped_label', or even
						 * a combination of attributes.
						 */
						COMPLICATED

					} type = Type::NO_LABEL;

					Checksum label_checksum { "" };

					Selector(Xml_node const &node)
					{
						bool const complicated =
							node.has_attribute("label_prefix") ||
							node.has_attribute("label_suffix") ||
							node.has_attribute("label_last")   ||
							node.has_attribute("unscoped_label");

						if (complicated) {
							type = Type::COMPLICATED;
							return;
						}

						Label const label = node.attribute_value("label", Label());
						if (label.valid()) {
							type           = Type::SPECIFIC_LABEL;
							label_checksum = Checksum(label);
						}
					}
				};

				Selector const _selector;
				Checksum const _service_checksum;
				bool     const _specific_service { _node.has_type("service") };

				struct Target : Noncopyable, private List<Target>::Element
				{
					friend class List<Target>;
					friend class Rule;

					Xml_node const node; /* points to 'Route_model::_route_node' */

					Target(Xml_node const &node) : node(node) { }
				};

				List<Target> _targets { };

				/**
				 * Constructor is private to 'Route_model'
				 */
				Rule(Allocator &alloc, Xml_node const &node)
				:
					_alloc(alloc), _node(node), _selector(node),
					_service_checksum(node.attribute_value("name", Service::Name()))
				{
					Target const *at_ptr = nullptr;
					node.for_each_sub_node([&] (Xml_node sub_node) {
						Target &target = *new (_alloc) Target(sub_node);
						_targets.insert(&target, at_ptr);
						at_ptr = &target;
					});
				}

				~Rule()
				{
					while (Target *target_ptr = _targets.first()) {
						_targets.remove(target_ptr);
						destroy(_alloc, target_ptr);
					}
				}

				/**
				 * Quick check for early detection of definite mismatches
				 *
				 * \return true if query definitely mismatches the rule,
				 *         false if the undecided
				 */
				bool _mismatches(Query const &query) const
				{
					if (_specific_service
					 && query.service_checksum != _service_checksum)
						return true;

					if (_selector.type == Selector::Type::SPECIFIC_LABEL
					 && query.label_checksum != _selector.label_checksum)
						return true;

					return false;
				}

			public:

				bool matches(Query const &query) const
				{
					/* handle common case */
					if (_mismatches(query))
						return false;

					return service_node_matches(_node,
					                            query.label,
					                            query.child,
					                            query.service);
				}

				template <typename FN>
				Child_policy::Route resolve(FN const &fn) const
				{
					for (Target const *t = _targets.first(); t; t = t->next()) {
						try { return fn(t->node); }
						catch (Service_denied) { /* try next target */ }
					}

					/* query is not accepted by any of the targets */
					throw Service_denied();
				}
		};

	private:

		Allocator &_alloc;

		Buffered_xml const _route_node;

		List<Rule> _rules { };

	public:

		Route_model(Allocator &alloc, Xml_node const &route)
		:
			_alloc(alloc), _route_node(_alloc, route)
		{
			Rule const *at_ptr = nullptr;
			_route_node.xml().for_each_sub_node([&] (Xml_node const &node) {
				Rule &rule = *new (_alloc) Rule(_alloc, node);
				_rules.insert(&rule, at_ptr); /* append */
				at_ptr = &rule;
			});
		}

		~Route_model()
		{
			while (Rule *rule_ptr = _rules.first()) {
				_rules.remove(rule_ptr);
				destroy(_alloc, rule_ptr);
			}
		}

		template <typename FN>
		Child_policy::Route resolve(Query const &query, FN const &fn) const
		{
			for (Rule const *r = _rules.first(); r; r = r->next())
				if (r->matches(query)) {
					try {
						return r->resolve(fn);
					}
					catch (Service_denied) {
						if (r->_specific_service)
							throw;

						/*
						 * If none of the targets of a wildcard rule was
						 * satisfied with the query, continue with the next
						 * rule.
						 */
					}
				}

			warning(query.child, ": no route to "
			        "service \"", query.service, "\" "
			        "(label=\"",  query.label, "\")");

			throw Service_denied();
		}
};

#endif /* _ROUTE_MODEL_H_ */
