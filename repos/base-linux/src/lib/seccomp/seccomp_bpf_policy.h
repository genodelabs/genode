/*
 * \brief  Including seccomp filter policy binary
 * \author Stefan Thoeni
 * \date   2019-12-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SECCOMP_BPF_POLICY_H_
#define _LIB__SECCOMP_BPF_POLICY_H_

#define STR2(x) #x
#define STR(x) STR2(x)

#define INCBIN(name, file) \
	__asm__(".section .rodata\n" \
	        ".global incbin_" STR(name) "_start\n" \
	        ".type incbin_" STR(name) "_start, @object\n" \
	        ".balign 16\n" \
	        "incbin_" STR(name) "_start:\n" \
	        ".incbin \"" file "\"\n" \
	        \
	        ".global incbin_" STR(name) "_end\n" \
	        ".type incbin_" STR(name) "_end, @object\n" \
	        ".balign 1\n" \
	        "incbin_" STR(name) "_end:\n" \
	        ".byte 0\n" \
	); \
	extern const __attribute__((aligned(16))) void* incbin_ ## name ## _start; \
	extern const void* incbin_ ## name ## _end; \

INCBIN(seccomp_bpf_policy, "seccomp_bpf_policy.bin");

#endif /* _LIB__SECCOMP_BPF_POLICY_H_ */
