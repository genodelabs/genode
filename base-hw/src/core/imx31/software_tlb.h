/*
 * \brief  Software TLB controls specific for the i.MX31
 * \author Norman Feske
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__IMX31__SOFTWARE_TLB_H_
#define _SRC__CORE__IMX31__SOFTWARE_TLB_H_

/* Genode includes */
#include <arm/v6/section_table.h>

/**
 * Software TLB controls
 */
class Software_tlb : public Arm_v6::Section_table { };

#endif /* _SRC__CORE__IMX31__SOFTWARE_TLB_H_ */

