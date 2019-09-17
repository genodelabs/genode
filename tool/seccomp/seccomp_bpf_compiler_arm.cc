/*
 * \brief  Generate seccomp filter policy for base-linux on arm
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

#include <stdio.h>   /* printf */
#include <seccomp.h> /* libseccomp */
#include "seccomp_bpf_compiler.h"

int main()
{
	Filter filter(SCMP_ARCH_ARM);
	return filter.create();
}
