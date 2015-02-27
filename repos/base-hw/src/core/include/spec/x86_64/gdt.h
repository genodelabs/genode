#ifndef _GDT_H_
#define _GDT_H_

#include <pseudo_descriptor.h>
#include <mtc_util.h>

extern int _mt_gdt_start;

namespace Genode
{
	/**
	 * Global Descriptor Table (GDT)
	 * See Intel SDM Vol. 3A, section 3.5.1
	 */
	class Gdt;
}

class Genode::Gdt
{
	public:

		/**
		 * Load GDT from mtc region into GDTR.
		 *
		 * \param virt_base  virtual base address of mode transition pages
		 */
		static void load(addr_t const virt_base)
		{
			asm volatile ("lgdt %0" : : "m" (Pseudo_descriptor (0x37,
						  _virt_mtc_addr(virt_base, (addr_t)&_mt_gdt_start))));
		}
};

#endif /* _GDT_H_ */
