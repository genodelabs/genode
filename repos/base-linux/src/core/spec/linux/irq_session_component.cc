/*
 * \brief  IRQ session implementation for base-linux
 * \author Johannes Kliemann
 * \date   2018-03-14
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <irq_session_component.h>

/* core-local includes */
#include <core_linux_syscalls.h>

using namespace Genode;


Irq_session_component::Irq_session_component(Range_allocator &, const char *)
:
	_irq_number(0), _irq_object(_irq_number)
{ }


Irq_session_component::~Irq_session_component()
{
	warning(__func__, " not implemented");
}


void Irq_session_component::ack_irq() { }


void Irq_session_component::sigh(Signal_context_capability) { }


Irq_session::Info Irq_session_component::info()
{
	return { .type = Irq_session::Info::Type::INVALID, .address = 0, .value = 0 };
}


Irq_object::Irq_object(unsigned irq)
:
	Thread(Weight::DEFAULT_WEIGHT, "irq", 4096 /* stack */, Type::NORMAL),
	_sig_cap(Signal_context_capability()), _irq(irq), _fd(-1)
{
	warning(__func__, " not implemented");
}


bool Irq_object::_associate()
{
	warning(__func__, " not implemented");
	return false;
}


void Irq_object::entry()
{
	warning(__func__, " not implemented");
}


void Irq_object::ack_irq()
{
	warning(__func__, " not implemented");
}


void Irq_object::start()
{
	warning(__func__, " not implemented");
}


void Irq_object::sigh(Signal_context_capability)
{
	warning(__func__, " not implemented");
}
