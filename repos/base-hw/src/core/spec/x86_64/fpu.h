/*
 * \brief  x86_64 FPU context
 * \author Sebastian Sumpf
 * \date   2019-05-23
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__FPU_H_
#define _CORE__SPEC__X86_64__FPU_H_

/* Genode includes */
#include <util/misc_math.h>
#include <util/mmio.h>
#include <util/string.h>

namespace Genode { class Fpu_context; }


class Genode::Fpu_context
{
	addr_t _fxsave_addr { 0 };

	/*
	 * FXSAVE area providing storage for x87 FPU, MMX, XMM,
	 * and MXCSR registers.
	 *
	 * For further details see Intel SDM Vol. 2A,
	 * 'FXSAVE instruction'.
	 */
	char   _fxsave_area[527];

	struct Context : Mmio<512>
	{
		struct Fcw   : Register<0, 16>  { };
		struct Mxcsr : Register<24, 32> { };

		Context(addr_t const base) : Mmio({(char *)base, Mmio::SIZE})
		{
			memset((void *)base, 0, Mmio::SIZE);
			write<Fcw>(0x37f);    /* mask exceptions SysV ABI */
			write<Mxcsr>(0x1f80);
		}
	};

	public:

		Fpu_context()
		{
			/* align to 16 byte boundary */
			_fxsave_addr = align_addr((addr_t)_fxsave_area, 4);

			/* set fcw/mxcsr */
			Context init(_fxsave_addr);
		}

		addr_t fpu_context() const { return _fxsave_addr; }
};

#endif /* _CORE__SPEC__X86_64__FPU_H_ */
