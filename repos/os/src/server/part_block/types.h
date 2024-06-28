/*
 * \brief  Types used by part_block
 * \author Christian Helmuth
 * \date   2023-03-16
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__TYPES_H_
#define _PART_BLOCK__TYPES_H_

/* Genode includes */
#include <base/registry.h>
#include <base/log.h>
#include <util/xml_generator.h>
#include <util/mmio.h>
#include <util/misc_math.h>
#include <util/utf8.h>
#include <block_session/connection.h>

namespace Block {
	using namespace Genode;

	struct Job;
	using Block_connection = Block::Connection<Job>;
}

#endif /* _PART_BLOCK__TYPES_H_ */
