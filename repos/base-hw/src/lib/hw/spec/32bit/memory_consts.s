/*
 * \brief   Kernel/Bootstrap constants shared in between C++/Assembler
 * \author  Stefan Kalkowski
 * \date    2024-07-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

##ifndef _SRC__LIB__HW__SPEC__64BIT_MEMORY_CONSTS_H_
##define _SRC__LIB__HW__SPEC__64BIT_MEMORY_CONSTS_H_

##include <base/stdint.h>

#namespace Hw {
#	namespace Mm {

HW_MM_KERNEL_START      = 0x80000000
HW_MM_KERNEL_STACK_SIZE = 0x10000

HW_MM_CPU_LOCAL_MEMORY_AREA_START         = 0xd0000000
HW_MM_CPU_LOCAL_MEMORY_AREA_SIZE          = 0x10000000
HW_MM_CPU_LOCAL_MEMORY_SLOT_SIZE          = 0x100000
HW_MM_CPU_LOCAL_MEMORY_SLOT_STACK_OFFSET  = 0x0
HW_MM_CPU_LOCAL_MEMORY_SLOT_OBJECT_OFFSET = 0x20000
HW_MM_CPU_LOCAL_MEMORY_SLOT_OBJECT_SIZE   = 0x2000

#	}
#}

##endif /* _SRC__LIB__HW__SPEC__64BIT_MEMORY_CONSTS_H_ */
