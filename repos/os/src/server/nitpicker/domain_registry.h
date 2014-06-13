/*
 * \brief  Domain registry
 * \author Norman Feske
 * \date   2014-06-12
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DOMAIN_REGISTRY_
#define _DOMAIN_REGISTRY_

#include <util/xml_node.h>
#include <util/color.h>

class Domain_registry
{
	public:

		class Entry : public Genode::List<Entry>::Element
		{
			public:

				typedef Genode::String<64> Name;
				typedef Genode::Color      Color;

			private:

				Name  _name;
				Color _color;

				friend class Domain_registry;

				Entry(Name const &name, Color color)
				:
					_name(name), _color(color)
				{ }

			public:

				bool has_name(Name const &name) const { return name == _name; }

				Color color() const { return _color; }
		};

		void _insert(Genode::Xml_node domain)
		{
			char buf[sizeof(Entry::Name)];
			buf[0] = 0;
			bool name_defined = false;
			try {
				domain.attribute("name").value(buf, sizeof(buf));
				name_defined = true;
			} catch (...) { }

			if (!name_defined) {
				PERR("no valid domain name specified");
				return;
			}

			Entry::Name const name(buf);

			if (lookup(name)) {
				PERR("domain name \"%s\" is not unique", name.string());
				return;
			}

			Entry::Color const color = domain.attribute_value("color", Entry::Color());

			_entries.insert(new (_alloc) Entry(name, color));
		}

	private:

		Genode::List<Entry> _entries;
		Genode::Allocator  &_alloc;

	public:

		Domain_registry(Genode::Allocator &alloc, Genode::Xml_node config)
		:
			_alloc(alloc)
		{
			char const *type = "domain";

			if (!config.has_sub_node(type))
				return;

			Genode::Xml_node domain = config.sub_node(type);

			for (;; domain = domain.next(type)) {

				_insert(domain);

				if (domain.is_last(type))
					break;
			}
		}

		~Domain_registry()
		{
			while (Entry *e = _entries.first())
				Genode::destroy(_alloc, e);
		}

		Entry const *lookup(Entry::Name const &name) const
		{
			for (Entry const *e = _entries.first(); e; e = e->next())
				if (e->has_name(name))
					return e;
			return 0;
		}
};

#endif /* _DOMAIN_REGISTRY_ */
