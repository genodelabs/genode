#ifndef _PSEUDO_DESCRIPTOR_H_
#define _PSEUDO_DESCRIPTOR_H_

#include <base/stdint.h>

namespace Genode
{
	/**
	 * Pseudo Descriptor
	 *
	 * See Intel SDM Vol. 3A, section 3.5.1
	 */
	class Pseudo_descriptor;
}

class Genode::Pseudo_descriptor
{
	private:
		uint16_t _limit;
		uint64_t _base;

	public:
		Pseudo_descriptor(uint16_t l, uint64_t b) : _limit(l), _base (b) { };
} __attribute__((packed));

#endif /* _PSEUDO_DESCRIPTOR_H_ */
