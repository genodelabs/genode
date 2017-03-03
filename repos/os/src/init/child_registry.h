/*
 * \brief  Child registry
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INIT__CHILD_REGISTRY_H_
#define _SRC__INIT__CHILD_REGISTRY_H_

/* local includes */
#include <child.h>
#include <name_registry.h>
#include <alias.h>
#include <report.h>

namespace Init { struct Child_registry; }


class Init::Child_registry : public Name_registry, Child_list
{
	private:

		List<Alias> _aliases;

		bool _unique(const char *name) const
		{
			/* check for name clash with an existing child */
			Genode::List_element<Init::Child> const *curr = first();
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
			if (!_unique(alias->name.string())) {
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

		Child::Name deref_alias(Child::Name const &name) override
		{
			for (Alias const *a = _aliases.first(); a; a = a->next())
				if (name == a->name)
					return a->child;

			return name;
		}
};

#endif /* _SRC__INIT__CHILD_REGISTRY_H_ */
