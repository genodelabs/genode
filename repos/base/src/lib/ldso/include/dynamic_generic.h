#ifndef _INCLUDE__DYNAMIC_GENERIC_H_
#define _INCLUDE__DYNAMIC_GENERIC_H_

/**
 * Offset of dynamic section of this ELF. This is filled out during
 * linkage by static linker.
 */
extern Elf::Addr _DYNAMIC;

namespace Linker {

	inline Elf::Addr dynamic_section_address()
	{
		return (Elf::Addr)&_DYNAMIC;
	}
}

#endif /* _INCLUDE__DYNAMIC_GENERIC_H_ */
