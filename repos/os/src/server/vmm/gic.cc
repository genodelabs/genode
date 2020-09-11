/*
 * \brief  VMM ARM Generic Interrupt Controller v2 device model
 * \author Stefan Kalkowski
 * \date   2019-08-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
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


void Gic::Gicd_banked::handle_irq()
{
	unsigned i = _cpu.state().irqs.virtual_irq;
	if (i > MAX_IRQ) return;

	irq(i).deassert();

	_cpu.state().irqs.virtual_irq = SPURIOUS;
}


bool Gic::Gicd_banked::pending_irq()
{
	Genode::Mutex::Guard guard(big_gic_lock());

	if (_cpu.state().irqs.virtual_irq != SPURIOUS) return true;

	Irq * i = _gic._pending_list.highest_enabled();
	Irq * j = _pending_list.highest_enabled();
	Irq * n = j;
	if (i && ((j && j->priority() > i->priority()) || !j)) n = i;
	if (!n) return false;
	_cpu.state().irqs.virtual_irq = n->number();
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

	_cpu.state().irqs.last_irq    = SPURIOUS;
	_cpu.state().irqs.virtual_irq = SPURIOUS;

	if (gic.version() >= 3) {
		_rdist.construct(GICR_MMIO_START +
		                 (cpu.cpu_id()*0x20000), 0x20000,
		                 cpu.cpu_id(),
		                 Vm::last_cpu() == cpu.cpu_id());
		bus.add(*_rdist);
	}
}


Register Gic::Irq_reg::read(Address_range & access, Cpu & cpu)
{
	Genode::Mutex::Guard guard(big_gic_lock());

	Register ret = 0;

	Register bits_per_irq = size * 8 / irq_count;
	for (unsigned i = (access.start * 8) / bits_per_irq;
	     i < ((access.start+access.size) * 8) / bits_per_irq; i++)
		ret |= read(cpu.gic().irq(i)) << ((i % (32/bits_per_irq) * bits_per_irq));
	return ret;
}


void Gic::Irq_reg::write(Address_range & access, Cpu & cpu, Register value)
{
	Genode::Mutex::Guard guard(big_gic_lock());

	Register bits_per_irq   = size * 8 / irq_count;
	Register irq_value_mask = (1<<bits_per_irq) - 1;
	for (unsigned i = (access.start * 8) / bits_per_irq;
	     i < ((access.start+access.size) * 8) / bits_per_irq; i++)
		write(cpu.gic().irq(i), (value >> ((i % (32/bits_per_irq))*bits_per_irq))
		                        & irq_value_mask);
}


unsigned Gic::version() { return _version; }


void Gic::Gicd_sgir::write(Address_range &, Cpu & cpu,
                           Mmio_register::Register value)
{
	Target_filter::Target_type type =
		(Target_filter::Target_type) Target_filter::get(value);
	unsigned target_list = Target_list::get(value);
	unsigned irq = Int_id::get(value);

	for (unsigned i = 0; i <= Vm::last_cpu(); i++) {
		switch (type) {
		case Target_filter::MYSELF:
			if (i != cpu.cpu_id()) { continue; }
			break;
		case Target_filter::ALL:
			if (i == cpu.cpu_id()) { continue; }
			break;
		case Target_filter::LIST:
			if (!(target_list & (1<<i))) { continue; }
			break;
		default:
			continue;
		};

		cpu.vm().cpu(i, [&] (Cpu & c) {
			c.gic().irq(irq).assert();
			if (cpu.cpu_id() != c.cpu_id()) { cpu.recall(); }
		});
	}
}


Register Gic::Gicd_itargetr::read(Address_range & access, Cpu & cpu)
{
	if (access.start < 0x20) { return (1 << cpu.cpu_id()) << 8; }
	return Irq_reg::read(access, cpu);
}


void Gic::Gicd_itargetr::write(Address_range & access, Cpu & cpu, Register value)
{
	if (access.start >= 0x20) { Irq_reg::write(access, cpu, value); }
}


Gic::Gic(const char * const      name,
         const Genode::uint64_t  addr,
         const Genode::uint64_t  size,
         unsigned                cpus,
         unsigned                version,
         Genode::Vm_connection & vm,
         Mmio_bus              & bus,
         Genode::Env           & env)
: Mmio_device(name, addr, size), _cpu_cnt(cpus), _version(version)
{
	add(_ctrl);
	add(_typer);
	add(_iidr);
	add(_igroupr);
	add(_isenabler);
	add(_csenabler);
	add(_ispendr);
	add(_icpendr);
	add(_isactiver);
	add(_icactiver);
	add(_ipriorityr);
	add(_itargetr);
	add(_icfgr);
	add(_irouter);
	add(_sgir);

	for (unsigned i = 0; i < (sizeof(Dummy::regs) / sizeof(Mmio_register)); i++)
		add(_reg_container.regs[i]);

	for (unsigned i = 0; i < MAX_SPI; i++)
		_spi[i].construct(i+MAX_SGI+MAX_PPI, Irq::SPI, _pending_list);

	bus.add(*this);

	if (version < 3) vm.attach_pic(GICC_MMIO_START);
}
