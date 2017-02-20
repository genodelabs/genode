/*
 * \brief  Mini C initialization
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MINI_C__INIT_H_
#define _MINI_C__INIT_H_

#include <base/allocator.h>

/**
 * Initialize malloc/free
 */
extern "C" void mini_c_init(Genode::Allocator &);

#endif /* _MINI_C__INIT_H_ */
