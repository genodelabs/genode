/*
 * \brief   Kernel backend for protection domains
 * \author  Stefan Kalkowski
 * \date    2015-03-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <cpu.h>
#include <kernel/thread.h>
#include <kernel/pd.h>

extern int __idt;
extern int __idt_end;

using namespace Core;


/**
 * Pseudo Descriptor
 *
 * See Intel SDM Vol. 3A, section 3.5.1
 */
struct Pseudo_descriptor
{
	uint16_t const limit = 0;
	uint64_t const base  = 0;

	constexpr Pseudo_descriptor(uint16_t l, uint64_t b) : limit(l), base(b) {}

} __attribute__((packed));


Cpu::Context::Context(bool core)
{
	eflags = EFLAGS_IF_SET;
	cs     = core ? 0x8 : 0x1b;
	ss     = core ? 0x10 : 0x23;
}


Cpu::Mmu_context::Mmu_context(addr_t table, Board::Address_space_id_allocator &)
: cr3(Cr3::Pdb::masked(table)) { }


void Cpu::Tss::init()
{
	enum { TSS_SELECTOR = 0x28, };
	asm volatile ("ltr %w0" : : "r" (TSS_SELECTOR));
}


void Cpu::Idt::init()
{
	Pseudo_descriptor descriptor {
		(uint16_t)((addr_t)&__idt_end - (addr_t)&__idt),
		(uint64_t)(&__idt) };
	asm volatile ("lidt %0" : : "m" (descriptor));
}


void Cpu::Gdt::init(addr_t tss_addr)
{
	tss_desc[0] = ((((tss_addr >> 24) & 0xff) << 24 |
	                ((tss_addr >> 16) & 0xff)       |
	               0x8900) << 32)                   |
	              ((tss_addr &  0xffff) << 16 | 0x68);
	tss_desc[1] = tss_addr >> 32;

	Pseudo_descriptor descriptor {
		(uint16_t)(sizeof(Gdt)),
		(uint64_t)(this) };
	asm volatile ("lgdt %0" :: "m" (descriptor));
}


void Cpu::mmu_fault(Context &regs, Kernel::Thread_fault &fault)
{
	using Fault = Kernel::Thread_fault::Type;

	/*
	 * Intel manual: 6.15 EXCEPTION AND INTERRUPT REFERENCE
	 *                    Interrupt 14—Page-Fault Exception (#PF)
	 */
	enum {
		ERR_I = 1UL << 4,
		ERR_R = 1UL << 3,
		ERR_U = 1UL << 2,
		ERR_W = 1UL << 1,
		ERR_P = 1UL << 0,
	};

	auto fault_lambda = [] (addr_t err) {
		if (err & ERR_W)    return Fault::WRITE;
		if (!(err & ERR_P)) return Fault::PAGE_MISSING;
		if (err & ERR_I)    return Fault::EXEC;
		else                return Fault::UNKNOWN;
	};

	fault.addr = Cpu::Cr2::read();
	fault.type = fault_lambda(regs.errcode);
}


extern void const * const kernel_stack;
extern size_t const kernel_stack_size;


bool Cpu::active(Mmu_context &mmu_context)
{
	return (mmu_context.cr3 == Cr3::read());
}


void Cpu::switch_to(Mmu_context &mmu_context)
{
	Cr3::write(mmu_context.cr3);
}


void Cpu::switch_to(Context &context)
{
	tss.ist[0] = (addr_t)&context + sizeof(Cpu_state);

	addr_t const stack_base = reinterpret_cast<addr_t>(&kernel_stack);
	context.kernel_stack = stack_base +
	                       (Cpu::executing_id() + 1) * kernel_stack_size -
	                       sizeof(addr_t);
}


unsigned Cpu::executing_id()
{
	void * const stack_ptr  = nullptr;
	addr_t const stack_addr = reinterpret_cast<addr_t>(&stack_ptr);
	addr_t const stack_base = reinterpret_cast<addr_t>(&kernel_stack);

	unsigned const cpu_id = (unsigned)((stack_addr - stack_base) / kernel_stack_size);

	return cpu_id;
}


void Cpu::clear_memory_region(addr_t const addr, size_t const size, bool)
{
	if (align_addr(addr, 3) == addr && align_addr(size, 3) == size) {
		addr_t start = addr;
		size_t count = size / 8;
		asm volatile ("rep stosq" : "+D" (start), "+c" (count)
		                          : "a" (0)  : "memory");
	} else {
		memset((void*)addr, 0, size);
	}
}


void Cpu::single_step(Context &regs, bool on)
{
	if (on)
		regs.eflags |= Context::Eflags::EFLAGS_TF;
	else
		regs.eflags &= ~Context::Eflags::EFLAGS_TF;
}
