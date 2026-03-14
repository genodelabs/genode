/*
 * \brief  Cached information about available launchers
 * \author Norman Feske
 * \date   2018-09-13
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__LAUNCHERS_H_
#define _MODEL__LAUNCHERS_H_

/* local includes */
#include <model/file_listing.h>

namespace Sculpt {

	struct Launchers : File_listing { using File_listing::File_listing; };
}

#endif /* _MODEL__LAUNCHERS_H_ */
