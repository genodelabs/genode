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

/* Genode includes */
#include <util/dictionary.h>

/* local includes */
#include <writer.h>

namespace Trace_recorder {
	class Backend_base;

	using Backend_name = Genode::String<64>;
	using Backends     = Genode::Dictionary<Backend_base, Backend_name>;
}


class Trace_recorder::Backend_base : Backends::Element
{
	protected:
		friend class Genode::Dictionary<Backend_base, Backend_name>;
		friend class Genode::Avl_node<Backend_base>;
		friend class Genode::Avl_tree<Backend_base>;

	public:
		using Name = Backend_name;
		using Backends::Element::name;

		Backend_base(Backends & backends, Name const &name)
		: Backends::Element(backends, name)
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
