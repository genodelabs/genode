/*
 * \brief  DDE iPXE SUPPORT API
 * \author Josef Soentgen
 * \date   2017-02-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _DDE_IPXE__SUPPORT_H_
#define _DDE_IPXE__SUPPORT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/env.h>

/**
 * Initialize DDE support
 *
 * This function needs to be called before any dde_ipxe_* function
 * is used.
 */
void dde_support_init(Genode::Env &, Genode::Allocator &alloc);

#endif /* _DDE_IPXE__SUPPORT_H_ */
