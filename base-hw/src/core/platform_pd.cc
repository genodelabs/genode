/*
 * \brief   Protection-domain facility
 * \author  Martin Stein
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform_pd.h>

using namespace Genode;


/*****************
 ** Platform PD **
 *****************/

void Platform_pd::unbind_thread(Platform_thread *thread) { assert(0); }


Platform_pd::~Platform_pd() { assert(0); }

