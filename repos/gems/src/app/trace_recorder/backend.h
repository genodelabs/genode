/*
 * \brief  Factory base for creating writers
 * \author Johannes Schlatow
 * \date   2022-05-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BACKEND_H_
#define _BACKEND_H_

/* local includes */
#include <named_registry.h>
#include <writer.h>

namespace Trace_recorder {
	class Backend_base;

	using Backends     = Named_registry<Backend_base>;
}


class Trace_recorder::Backend_base : Backends::Element
{
	protected:
		friend class Backends::Element;
		friend class Avl_node<Backend_base>;
		friend class Avl_tree<Backend_base>;

	public:

		using Name = Backends::Element::Name;
		using Backends::Element::name;
		using Backends::Element::Element;

		Backend_base(Backends & registry, Name const &name)
		: Backends::Element(registry, name)
		{ }

		virtual ~Backend_base() { }

		/***************
		 ** Interface **
		 ***************/

		virtual Writer_base &create_writer(Genode::Allocator &,
		                                   Genode::Registry<Writer_base> &,
		                                   Directory &,
		                                   Directory::Path const &) = 0;
};

#endif /* _BACKEND_H_ */
