/*
 * \brief  Platform specific part of the Linux API emulation
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _X86_32__PLATFORM__LX_EMUL_
#define _X86_32__PLATFORM__LX_EMUL_


/*******************
 ** asm/barrier.h **
 *******************/

#include <lx_emul/barrier.h>


struct platform_device
{
	void *data;
};


#define dev_is_pci(d) (1)

#endif /* _X86_32__PLATFORM__LX_EMUL_ */
