/*
 * \brief  Block session initialize function
 * \author Josef Soentgen
 * \date   2017-03-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FATFS__BLOCK_H_
#define _INCLUDE__FATFS__BLOCK_H_

namespace Genode {
	struct Env;
	struct Allocator;
}

namespace Fatfs {
	void block_init(Genode::Env &, Genode::Allocator &heap);
}

#endif /* _INCLUDE__FATFS__BLOCK_H_ */
