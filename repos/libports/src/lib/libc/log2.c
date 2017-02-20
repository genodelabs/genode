/*
 * \brief  Preliminary 'log2()' and 'log2f()' implementations
 * \author Christian Prochaska
 * \date   2012-03-06
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <math.h>


double log2(double x)
{
	return log(x) / log(2);
}


float log2f(float x)
{
	return logf(x) / logf(2);
}
