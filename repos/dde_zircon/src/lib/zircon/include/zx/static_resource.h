/*
 * \brief  Utility to provide resources via type based static singletons
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ZX_STATIC_RESOURCE_H_
#define _ZX_STATIC_RESOURCE_H_

#include <base/exception.h>
#include <util/reconstructible.h>

namespace ZX
{
	template <typename T>
	class Resource_uninitialized : Genode::Exception {};

	template <typename T>
	class Resource_already_initialized : Genode::Exception {};

	template <typename Component>
	class Container
	{
		private:

			Component &_component;

		public:

			Container(Component &component) : _component(component)
			{ }

			Component &component()
			{
				return _component;
			}
	};

	template <typename Component>
	class Resource
	{
		private:

			static Genode::Constructible<Container<Component>> _container;

			Resource() {}

		public:

			static Component &get_component()
			{
				if (_container.constructed()){
					return _container->component();
				}
				Genode::error("Uninitialized resource: ", __PRETTY_FUNCTION__);
				throw Resource_uninitialized<Component>();
			}

			static void set_component(Component &component)
			{
				if (!_container.constructed()){
					_container.construct(component);
					return;
				}
				Genode::error("Already initialized resource: ", __PRETTY_FUNCTION__);
				throw Resource_already_initialized<Component>();
			}

			static bool initialized()
			{
				return _container.constructed();
			}

	};

}

template <typename Component>
Genode::Constructible<ZX::Container<Component>> ZX::Resource<Component>::_container;

#endif /* ifndef _ZX_STATIC_RESOURCE_H_ */
