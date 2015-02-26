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
		 * Return virtual mtc address of the given label for the specified
		 * virtual mode transition base.
		 *
		 * \param virt_base  virtual address of the mode transition pages
		 */
		static addr_t _virt_mtc_addr(addr_t const virt_base, addr_t const label);

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
		 *
		 * \param virt_base  virtual base address of mode transition pages
		 */
		void load(addr_t const virt_base);
};

#endif /* _IDT_H_ */
