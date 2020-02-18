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
#include <irq_object.h>

#include <core_linux_syscalls.h>

Genode::Irq_session_component::Irq_session_component(Genode::Range_allocator &, const char *args)
:
	_irq_number(Arg_string::find_arg(args, "irq_number").long_value(-1)),
	_irq_object(_irq_number)
{
	_irq_object.start();
}

Genode::Irq_session_component::~Irq_session_component()
{
	warning(__func__, " not implemented");
}

void Genode::Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}

void Genode::Irq_session_component::sigh(Genode::Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}

Genode::Irq_session::Info Genode::Irq_session_component::info()
{
	return { .type = Genode::Irq_session::Info::Type::INVALID, .address = 0, .value = 0 };
}


Genode::Irq_object::Irq_object(unsigned irq) :
	Thread_deprecated<4096>("irq"),
	_sig_cap(Signal_context_capability()),
	_irq(irq),
	_fd(-1)
{ }

bool Genode::Irq_object::_associate()
{
	_fd = lx_open("/dev/hwio", O_RDWR | O_SYNC);

	if (_fd < 0){
		Genode::error("failed to open /dev/hwio");
		return false;
	}

	if (lx_ioctl_irq(_fd, _irq) < 0){
		Genode::error("failed to request irq");
		return false;
	}

	return true;
}

void Genode::Irq_object::entry()
{
	if (!_associate()) {
		error("failed to register IRQ ", _irq);
	}

	_sync_bootup.wakeup();
	_sync_ack.block();

	while (true) {

		if (lx_read(_fd, 0, 0) < 0) {
			Genode::warning("failed to read on /dev/hwio");
		}

		if(!_sig_cap.valid()){
			continue;
		}

		Genode::Signal_transmitter(_sig_cap).submit(1);

		_sync_ack.block();
	}
}

void Genode::Irq_object::ack_irq()
{
	_sync_ack.wakeup();
}

void Genode::Irq_object::start()
{
	Genode::Thread::start();
	_sync_bootup.block();
}

void Genode::Irq_object::sigh(Signal_context_capability cap)
{
	_sig_cap = cap;
}


