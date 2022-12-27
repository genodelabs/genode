/*
 * \brief  Registry for storing interfaces description blocks
 * \author Johannes Schlatow
 * \date   2022-05-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__INTERFACE_REGISTRY_H_
#define _PCAPNG__INTERFACE_REGISTRY_H_

/* local includes */
#include <base/registry.h>
#include <pcapng/interface_description_block.h>

namespace Pcapng {
	using namespace Trace_recorder;
	using namespace Genode;

	class Interface;
	class Interface_registry;
}


class Pcapng::Interface
{
	public:

		using Name = String<Interface_name::MAX_NAME_LEN>;

	private:

		Name     const               _name;
		unsigned const               _id;

		Registry<Interface>::Element _element;

	public:

		Interface(Name const &name, unsigned id, Registry<Interface> &registry)
		: _name(name),
		  _id(id),
		  _element(registry, *this)
		{ }

		/*************
		 * Accessors *
		 *************/

		unsigned    id()   const { return _id; }
		Name const &name() const { return _name; }
};


class Pcapng::Interface_registry : private Registry<Interface>
{
	private:

		unsigned   _next_id { 0 };
		Allocator &_alloc;

	public:

		Interface_registry(Allocator &alloc)
		: _alloc(alloc)
		{ }

		/* apply to existing Interface or create new one */
		template <typename FUNC_EXISTS, typename FUNC_NEW>
		void from_name(Interface_name const &name, FUNC_EXISTS && fn_exists, FUNC_NEW && fn_new)
		{
			bool found = false;
			for_each([&] (Interface const &iface) {
				if (iface.name() == name.string()) {
					found = true;
					fn_exists(iface);
				}
			});

			/* create new interface */
			if (!found) {
				if (fn_new(name, _next_id))
					new (_alloc) Interface(Interface::Name(name.string()), _next_id++, *this);
			}
		}

		void clear()
		{
			for_each([&] (Interface &iface) { destroy(_alloc, &iface); });

			_next_id = 0;
		}
};


#endif /* _PCAPNG__INTERFACE_REGISTRY_H_ */
