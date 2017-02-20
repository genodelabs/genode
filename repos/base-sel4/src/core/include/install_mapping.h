/*
 * \brief   Interface for mapping memory to the VM space of a pager object
 * \author  Norman Feske
 * \date    2015-05-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__INSTALL_MAPPING_H_
#define _CORE__INCLUDE__INSTALL_MAPPING_H_

/* Core includes */
#include <ipc_pager.h>
#include <pager.h>

namespace Genode {

	void install_mapping(Mapping const &mapping, unsigned long pager_object_badge);
}

#endif /* _CORE__INCLUDE__INSTALL_MAPPING_H_ */
