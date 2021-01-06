/*
 * \brief  OpenSSL ARM CPU capabilities (with NEON support)
 * \author Pirmin Duss
 * \date   2021-02-17
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <stdint.h>
#include "arm_arch.h"


unsigned int OPENSSL_armcap_P = ARMV7_NEON;

void OPENSSL_cpuid_setup(void) { }

uint32_t OPENSSL_rdtsc(void) { return 0; }
