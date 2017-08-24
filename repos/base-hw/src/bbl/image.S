/**
 * \brief  Placeholder for 'image.elf'
 * \author Sebastian Sumpf
 * \date   2017-08-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section ".payload","a",@progbits
.align 3

.globl _payload_start, _payload_end
_payload_start:
#ifdef BBL_PAYLOAD
.incbin BBL_PAYLOAD
#endif
_payload_end:
