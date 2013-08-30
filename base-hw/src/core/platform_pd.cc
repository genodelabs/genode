/*
 * \brief   Protection-domain facility
 * \author  Martin Stein
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
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

Platform_pd::~Platform_pd()
{
	/*
	 * FIXME: throwing exceptions is not declared for
	 *        'Pd_root::close' wich is why we can only
	 *        print an error
	 */
	PERR("not implemented");
}

