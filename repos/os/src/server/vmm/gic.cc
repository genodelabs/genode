/*
 * \brief  VMM ARM Generic Interrupt Controller v2 device model
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2019-08-05
 */

/*
 * Copyright (C) 2019-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>
#include <gic.h>
#include <vm.h>

using Vmm::Gic;
using Register = Vmm::Mmio_register::Register;

static Genode::Mutex & big_gic_lock()
{
	static Genode::Mutex mutex;
	return mutex;
}


bool Gic::Irq::enabled() const { return _enabled; }

bool Gic::Irq::active()  const {
	return _state == ACTIVE || _state == ACTIVE_PENDING; }

bool Gic::Irq::pending() const {
	return _state == PENDING || _state == ACTIVE_PENDING; }

bool Gic::Irq::level() const { return _config == LEVEL; }

unsigned Gic::Irq::number()  const { return _num; }

Genode::uint8_t Gic::Irq::priority() const { return _prio; }

Genode::uint8_t Gic::Irq::target() const { return _target; }


void Gic::Irq::enable()
{
	_enabled = true;
	if (_handler) _handler->enabled();
}


void Gic::Irq::disable()
{
	_enabled = false;
	if (_handler) _handler->disabled();
}


void Gic::Irq::activate()
{
	switch (_state) {
	case INACTIVE:       _state = ACTIVE;         return;
	case PENDING:        _state = ACTIVE_PENDING; return;
	case ACTIVE_PENDING: return;
	case ACTIVE:         return;
	};
}


void Gic::Irq::deactivate()
{
	switch (_state) {
	case INACTIVE:       return;
	case PENDING:        return;
	case ACTIVE_PENDING: _state = PENDING;  return;
	case ACTIVE:         _state = INACTIVE; return;
	};
}


void Gic::Irq::assert()
{
	if (pending()) return;

	Genode::Mutex::Guard guard(big_gic_lock());

	_state = PENDING;
	_pending_list.insert(*this);
}


void Gic::Irq::deassert()
{
	if (_state == INACTIVE) return;

	Genode::Mutex::Guard guard(big_gic_lock());

	_state = INACTIVE;
	_pending_list.remove(this);
	if (_handler) _handler->eoi();
}


void Gic::Irq::target(Genode::uint8_t t)
{
	if (_target == t) return;

	_target = t;
}


void Gic::Irq::level(bool l)
{
	if (level() == l) return;

	_config = l ? LEVEL : EDGE;
}


void Gic::Irq::priority(Genode::uint8_t p)
{
	if (_prio == p) return;

	_prio = p;
}


void Gic::Irq::handler(Gic::Irq::Irq_handler & handler) { _handler = &handler; }


Gic::Irq::Irq(unsigned num, Type t, Irq::List & l)
: _type(t), _num(num), _pending_list(l) {}


void Gic::Irq::List::insert(Irq & irq)
{
	Irq * i = first();
	while (i && i->priority() <= irq.priority() && i->next()) i = i->next();
	Genode::List<Irq>::insert(&irq, i);
}


Gic::Irq * Gic::Irq::List::highest_enabled(unsigned cpu_id)
{
	for (Irq * i = first(); i; i = i->next()) {
		if (!i->enabled() || i->active()) { continue; }
		if (cpu_id < ~0U && (i->target() != cpu_id)) { continue; }
		return i;
	}
	return nullptr;
}


Gic::Irq & Gic::Gicd_banked::irq(unsigned i)
{
	if (i < MAX_SGI)
		return *_sgi[i];

	if (i < MAX_SGI+MAX_PPI)
		return *_ppi[i-MAX_SGI];

	return *_gic._spi[i-MAX_SGI-MAX_PPI];
}


void Gic::Gicd_banked::handle_irq(Vcpu_state &state)
{
	unsigned i = state.irqs.virtual_irq;
	if (i > MAX_IRQ) return;

	irq(i).deassert();

	state.irqs.virtual_irq = SPURIOUS;
}


bool Gic::Gicd_banked::pending_irq(Vcpu_state &state)
{
	Genode::Mutex::Guard guard(big_gic_lock());

	if (state.irqs.virtual_irq != SPURIOUS)
		return true;

	Irq * i = _gic._pending_list.highest_enabled();
	Irq * j = _pending_list.highest_enabled();
	Irq * n = j;
	if (i && ((j && j->priority() > i->priority()) || !j)) n = i;
	if (!n) return false;
	state.irqs.virtual_irq = n->number();
	n->activate();
	return true;
}


Gic::Gicd_banked::Gicd_banked(Cpu_base & cpu, Gic & gic, Mmio_bus & bus)
: _cpu(cpu), _gic(gic)
{
	for (unsigned i = 0; i < MAX_SGI; i++)
		_sgi[i].construct(i, Irq::SGI, _pending_list);

	for (unsigned i = 0; i < MAX_PPI; i++)
		_ppi[i].construct(i+MAX_SGI, Irq::PPI, _pending_list);


	if (gic.version() >= 3) {
		_rdist.construct(GICR_MMIO_START +
		                 (cpu.cpu_id()*0x20000), 0x20000, bus,
		                 cpu.cpu_id(),
		                 (_gic._cpu_cnt-1) == cpu.cpu_id());
	}
}

void Gic::Gicd_banked::setup_state(Vcpu_state &state)
{
	state.irqs.last_irq    = SPURIOUS;
	state.irqs.virtual_irq = SPURIOUS;
}


Register Gic::Irq_reg::read(Address_range & access, Cpu & cpu)
{
	Genode::Mutex::Guard guard(big_gic_lock());

	Register ret = 0;

	for_range(access, [&] (unsigned i, Register bits_per_irq) {
		ret |= read(cpu.gic().irq((unsigned)i))
		       << ((i % (32/bits_per_irq) * bits_per_irq)); });
	return ret;
}


void Gic::Irq_reg::write(Address_range & access, Cpu & cpu, Register value)
{
	Genode::Mutex::Guard guard(big_gic_lock());

	for_range(access, [&] (unsigned i, Register bits_per_irq) {
		write(cpu.gic().irq((unsigned)i),
		      (value >> ((i % (32/bits_per_irq))*bits_per_irq))
		      & ((1<<bits_per_irq) - 1)); });
}


unsigned Gic::version() { return _version; }


void Gic::Gicd_sgir::write(Address_range &, Cpu & cpu,
                           Mmio_register::Register value)
{
	using Target_filter = Reg::Target_filter;

	Target_filter::Target_type type = (Target_filter::Target_type)
		Target_filter::get((Reg::access_t)value);
	unsigned target_list = Reg::Target_list::get((Reg::access_t)value);
	unsigned irq = Reg::Int_id::get((Reg::access_t)value);

	cpu.vm().for_each_cpu([&] (Cpu & c) {
		switch (type) {
		case Target_filter::MYSELF:
			if (c.cpu_id() != cpu.cpu_id())
				return;
			break;
		case Target_filter::ALL:
			if (c.cpu_id() == cpu.cpu_id())
				return;
			break;
		case Target_filter::LIST:
			if (!(target_list & (1<<c.cpu_id())))
				return;
			break;
		default:
			return;
		};

		c.gic().irq(irq).assert();
		if (cpu.cpu_id() != c.cpu_id()) { cpu.recall(); }
	});
}


Register Gic::Gicd_itargetr::read(Address_range & access, Cpu & cpu)
{
	if (access.start() < 0x20) { return (1 << cpu.cpu_id()) << 8; }
	return Irq_reg::read(access, cpu);
}


void Gic::Gicd_itargetr::write(Address_range & access, Cpu & cpu, Register value)
{
	if (access.start() >= 0x20) { Irq_reg::write(access, cpu, value); }
}


Gic::Gic(const char * const      name,
         const Genode::uint64_t  addr,
         const Genode::uint64_t  size,
         unsigned                cpus,
         unsigned                version,
         Genode::Vm_connection & vm,
         Space                 & bus,
         Genode::Env           &)
:
	Mmio_device(name, addr, size, bus),
	_cpu_cnt(cpus),
	_version(version)
{
	for (unsigned i = 0; i < MAX_SPI; i++)
		_spi[i].construct(i+MAX_SGI+MAX_PPI, Irq::SPI, _pending_list);

	if (version < 3) vm.attach_pic(GICC_MMIO_START);
}
