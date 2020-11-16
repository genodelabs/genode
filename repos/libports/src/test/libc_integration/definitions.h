/*
 * \brief  common definitions
 * \author Pirmin Duss
 * \date   2020-11-11
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef INTEGRATION_TEST_TYPES_H
#define INTEGRATION_TEST_TYPES_H


/* libc includes */
#include <algorithm>
#include <utility>


namespace Integration_test
{
	using namespace std;

	enum {
		BUFFER_SIZE       = 256 * 1024,
		IN_DATA_SIZE      = 8192 * 2,
		NUMBER_OF_WORKERS = 200000000,
		PARALLEL_WORKERS  = 23,
		WRITE_SIZE        = 1024,
	};
}


#endif  /* INTEGRATION_TEST_TYPES_H */
