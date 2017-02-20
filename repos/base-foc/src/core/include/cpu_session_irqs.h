/*
 * \brief  Fiasco.OC-specific implementation of core's CPU service
 * \author Christian Helmuth
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CPU_SESSION_IRQS_H_
#define _CORE__INCLUDE__CPU_SESSION_IRQS_H_

/* Genode includes */
#include <util/avl_tree.h>

/* core includes */
#include <cpu_session_component.h>

namespace Genode { class Cpu_session_irqs; }


class Genode::Cpu_session_irqs : public Avl_node<Cpu_session_irqs>
{
	private:

		enum { IRQ_MAX = 20 };

		Cpu_session_component* _owner;
		Native_capability      _irqs[IRQ_MAX];
		unsigned               _cnt;

	public:

		Cpu_session_irqs(Cpu_session_component *owner)
		: _owner(owner), _cnt(0) {}

		bool add(Native_capability irq)
		{
			if (_cnt >= (IRQ_MAX - 1))
				return false;
			_irqs[_cnt++] = irq;
			return true;
		}

		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Cpu_session_irqs *c) { return (c->_owner > _owner); }

		Cpu_session_irqs *find_by_session(Cpu_session_component *o)
		{
			if (o == _owner) return this;

			Cpu_session_irqs *c = Avl_node<Cpu_session_irqs>::child(o > _owner);
			return c ? c->find_by_session(o) : 0;
		}
};

#endif /* _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_ */
