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
			SIZE_IDT = 256,
		};

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
		static gate _table[];

	public:

		/**
		 * Setup IDT.
		 */
		static void setup();

		/**
		 * Load IDT into IDTR.
		 */
		static void load();
};

#endif /* _IDT_H_ */
