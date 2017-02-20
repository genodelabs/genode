/*
 * \brief  Port of ACPICA library
 * \author Alexander Boettcher
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ACPICA__ACPICA_H_
#define _INCLUDE__ACPICA__ACPICA_H_

namespace Genode {
	struct Env;
	struct Allocator;
}

namespace Acpica { void init(Genode::Env &env, Genode::Allocator &heap); }

#endif /* _INCLUDE__ACPICA__ACPICA_H_ */
