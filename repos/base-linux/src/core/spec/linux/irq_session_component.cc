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
#include <base/thread.h>
#include <irq_session_component.h>

/* core includes */
#include <core_linux_syscalls.h>

using namespace Core;


Irq_session_component::Irq_session_component(Runtime &runtime,
                                             Range_allocator &, const char *)
:
	_irq_number(0), _irq_object(runtime, _irq_number)
{ }


Irq_session_component::~Irq_session_component() { }


void Irq_session_component::ack_irq() { }


void Irq_session_component::sigh(Signal_context_capability) { }


Irq_session::Info Irq_session_component::info()
{
	return { .type = Irq_session::Info::Type::INVALID, .address = 0, .value = 0 };
}


Irq_object::Irq_object(Runtime &runtime, unsigned irq)
:
	Thread(runtime, "irq", Stack_size { 4096 }, Thread::Location { }),
	_sig_cap(Signal_context_capability()), _irq(irq), _fd(-1)
{ }


bool Irq_object::_associate() { return false; }
void Irq_object::entry() { }
void Irq_object::ack_irq() { }


Thread::Start_result Irq_object::start()
{
	return Start_result::DENIED;
}


void Irq_object::sigh(Signal_context_capability) { }
