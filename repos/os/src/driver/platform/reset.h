/*
 * \brief  Reset-domain interface
 * \author Norman Feske
 * \date   2021-11-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RESET_H_
#define _RESET_H_

#include <types.h>
#include <named_registry.h>

namespace Driver {

	using namespace Genode;

	struct Reset;

	using Resets = Named_registry<Reset>;
}


class Driver::Reset : Resets::Element, Interface
{
	private:

		/* friendships needed to make 'Resets::Element' private */
		friend class Resets::Element;
		friend class Genode::Avl_node<Reset>;
		friend class Genode::Avl_tree<Reset>;

		Switch<Reset> _switch { *this, &Reset::_deassert, &Reset::_assert };

	protected:

		virtual void _deassert() { }
		virtual void _assert()   { }

	public:

		using Name = Resets::Element::Name;
		using Resets::Element::name;
		using Resets::Element::Element;

		void deassert() { _switch.use();   }
		void assert()   { _switch.unuse(); }

		struct Guard : Genode::Noncopyable
		{
			Reset &_reset;
			Guard(Reset &reset) : _reset(reset) { _reset.deassert(); }
			~Guard() { _reset.assert(); }
		};
};

#endif /* _RESET_H_ */
