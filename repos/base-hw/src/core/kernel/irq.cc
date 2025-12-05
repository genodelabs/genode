/*
 * \brief   Kernel back-end and core front-end for user interrupts
 * \author  Martin Stein
 * \date    2013-10-28
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/irq.h>

using namespace Kernel;

void Irq::disable() const
{
	_pic.mask(_id);
}


void Irq::enable() const
{
	_pic.unmask(_id, Cpu::executing_id());
}


Irq::Irq(unsigned const id, Pool &pool, Controller &pic)
:
	_id(id),
	_pool(pool),
	_pic(pic)
{
	_pool._tree.insert(this);
}


Irq::~Irq() { _pool._tree.remove(this); }


void User_irq::occurred()
{
	_context.submit(1);
	disable();
}


User_irq::User_irq(unsigned const id, Trigger trigger, Polarity polarity,
                   Signal_context &context, Controller &pic, Pool &pool)
:
	Irq(id, pool, pic),
	_context(context)
{
	disable();
	_pic.irq_mode(id, trigger, polarity);
}


User_irq::~User_irq() { disable(); }
