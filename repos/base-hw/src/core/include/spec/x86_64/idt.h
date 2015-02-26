#ifndef _IDT_H_
#define _IDT_H_

#include <base/stdint.h>

namespace Genode
{
	/**
	 * Interrupt Descriptor Table (IDT)
	 * See Intel SDM Vol. 3A, section 6.10
	 */
	class Idt;
}

class Genode::Idt
{
	private:

		enum {
			SIZE_IDT    = 256,
			SYSCALL_VEC = 0x80,
		};

		/**
		 * Return virtual address of the IDT for given virtual mode transition
		 * base.
		 */
		static addr_t _virt_idt_addr(addr_t const virt_base);

		/**
		 * 64-Bit Mode IDT gate, see Intel SDM Vol. 3A, section 6.14.1.
		 */
		struct gate
		{
			uint16_t offset_15_00;
			uint16_t segment_sel;
			uint16_t flags;
			uint16_t offset_31_16;
			uint32_t offset_63_32;
			uint32_t reserved;
		};

		/**
		 * IDT table
		 */
		__attribute__((aligned(8))) gate _table[SIZE_IDT];

	public:

		/**
		 * Setup IDT.
		 */
		void setup();

		/**
		 * Load IDT into IDTR.
		 */
		void load();
};

#endif /* _IDT_H_ */
