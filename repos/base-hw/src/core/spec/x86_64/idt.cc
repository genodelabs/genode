#include "idt.h"

using namespace Genode;

extern uint64_t _isr_array[];

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
	uint64_t *isrs = _isr_array;

	for (unsigned vec = 0; vec < SIZE_IDT; vec++, isrs++) {
		_table[vec].offset_15_00 = *isrs & 0xffff;
		_table[vec].segment_sel  = 8;
		_table[vec].flags        = 0x8e00;
		_table[vec].offset_31_16 = (*isrs >> 16) & 0xffff;
		_table[vec].offset_63_32 = (*isrs >> 32) & 0xffff;
	}

	/* Set DPL of syscall entry to 3 */
	_table[SYSCALL_VEC].flags = _table[SYSCALL_VEC].flags | 0x6000;
}


void Idt::load()
{
	asm volatile ("lidt %0" : : "m" (Descriptor (sizeof (_table) - 1,
				  reinterpret_cast<uint64_t>(_table))));
}
