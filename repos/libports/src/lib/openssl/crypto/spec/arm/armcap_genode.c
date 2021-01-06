/*
 * \brief  OpenSSL ARM CPU capabilities (no NEON)
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


unsigned int OPENSSL_armcap_P = 0;

void OPENSSL_cpuid_setup(void) { }

uint32_t OPENSSL_rdtsc(void) { return 0; }
