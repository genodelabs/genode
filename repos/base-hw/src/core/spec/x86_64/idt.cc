#include "idt.h"

using namespace Genode;

class Descriptor
{
	private:
		uint16_t _limit;
		uint64_t _base;

	public:
		Descriptor(uint16_t l, uint64_t b) : _limit(l), _base (b) {};
} __attribute__((packed));

__attribute__((aligned(8))) Idt::gate Idt::_table[SIZE_IDT];


void Idt::setup()
{
	/* TODO: Calculate from _mt_isrs label */
	uint64_t base = 0;

	for (unsigned vec = 0; vec < SIZE_IDT; vec++) {
		/* ISRs are padded to 4 bytes */
		base = vec * 0xc;

		_table[vec].offset_15_00 = base & 0xffff;
		_table[vec].segment_sel  = 8;
		_table[vec].flags        = 0x8e00;
		_table[vec].offset_31_16 = (base >> 16) & 0xffff;
		_table[vec].offset_63_32 = (base >> 32) & 0xffff;
	}

	/* Set DPL of syscall entry to 3 */
	_table[SYSCALL_VEC].flags = _table[SYSCALL_VEC].flags | 0x6000;
}


void Idt::load()
{
	asm volatile ("lidt %0" : : "m" (Descriptor (sizeof (_table) - 1,
				  reinterpret_cast<uint64_t>(_table))));
}
