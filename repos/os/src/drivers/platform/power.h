/*
 * \brief  Power-domain interface
 * \author Norman Feske
 * \date   2021-11-18
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POWER_H_
#define _POWER_H_

#include <types.h>
#include <named_registry.h>

namespace Driver {

	using namespace Genode;

	struct Power;

	using Powers = Named_registry<Power>;
}


class Driver::Power : Powers::Element, Interface
{
	private:

		/* friendships needed to make 'Powers::Element' private */
		friend class Powers::Element;
		friend class Genode::Avl_node<Power>;
		friend class Genode::Avl_tree<Power>;

		Switch<Power> _switch { *this, &Power::_on, &Power::_off };

	protected:

		virtual void _on()  { }
		virtual void _off() { }

	public:

		using Name = Powers::Element::Name;
		using Powers::Element::name;
		using Powers::Element::Element;

		void on()  { _switch.use();   }
		void off() { _switch.unuse(); }

		struct Guard : Genode::Noncopyable
		{
			Power &_power;
			Guard(Power &power) : _power(power) { _power.on(); }
			~Guard() { _power.off(); }
		};
};

#endif /* _POWER_H_ */
