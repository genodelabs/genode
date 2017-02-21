/*
 * \brief  Interrupt controller definitions for ARM
 * \author Stefan Kalkowski
 * \date   2017-02-22
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARM__PIC_H_
#define _SRC__BOOTSTRAP__SPEC__ARM__PIC_H_

#include <hw/spec/arm/pic.h>

namespace Bootstrap { struct Pic; }

struct Bootstrap::Pic : Hw::Pic
{
	void init_cpu_local();
};

#endif /* _SRC__BOOTSTRAP__SPEC__ARM__PIC_H_ */
