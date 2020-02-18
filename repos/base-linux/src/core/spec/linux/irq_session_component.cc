/*
 * \brief IRQ session implementation for base-linux
 * \author Johannes Kliemann
 * \date 2018-03-14
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/thread.h>

#include <irq_session_component.h>

#include <core_linux_syscalls.h>

Genode::Irq_session_component::Irq_session_component(Genode::Range_allocator &, const char *)
:
	_irq_number(0),
	_irq_object(_irq_number)
{ }

Genode::Irq_session_component::~Irq_session_component()
{
	warning(__func__, " not implemented");
}

void Genode::Irq_session_component::ack_irq()
{ }

void Genode::Irq_session_component::sigh(Genode::Signal_context_capability)
{ }

Genode::Irq_session::Info Genode::Irq_session_component::info()
{
	return { .type = Genode::Irq_session::Info::Type::INVALID, .address = 0, .value = 0 };
}

Genode::Irq_object::Irq_object(unsigned irq) :
	Thread_deprecated<4096>("irq"),
	_sig_cap(Signal_context_capability()),
	_irq(irq),
	_fd(-1)
{
    Genode::warning(__func__, " not implemented");
}

bool Genode::Irq_object::_associate()
{
    Genode::warning(__func__, " not implemented");
    return false;
}

void Genode::Irq_object::entry()
{
    Genode::warning(__func__, " not implemented");
}

void Genode::Irq_object::ack_irq()
{
    Genode::warning(__func__, " not implemented");
}

void Genode::Irq_object::start()
{
    Genode::warning(__func__, " not implemented");
}

void Genode::Irq_object::sigh(Signal_context_capability)
{
    Genode::warning(__func__, " not implemented");
}


