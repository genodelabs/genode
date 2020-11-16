/*
 * \brief  Wrapper for stdcxx output handling.
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


/* local includes */
#include "stdcxx_log.h"

std::mutex Integration_test::lock { };
