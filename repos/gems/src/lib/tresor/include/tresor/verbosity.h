/*
 * \brief  Verbosity configuration
 * \author Martin Stein
 * \date   2023-05-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VERBOSITY_H_
#define _TRESOR__VERBOSITY_H_

namespace Tresor {

	enum { VERBOSE_MODULE_COMMUNICATION = 0 };
	enum { VERBOSE_VBD_EXTENSION        = 0 };
	enum { VERBOSE_FT_EXTENSION         = 0 };
	enum { VERBOSE_REKEYING             = 0 };
	enum { VERBOSE_READ_VBA             = 0 };
	enum { VERBOSE_WRITE_VBA            = 0 };
	enum { VERBOSE_CRYPTO               = 0 };
	enum { VERBOSE_BLOCK_IO             = 0 };
	enum { VERBOSE_BLOCK_IO_PBA_FILTER  = 0 };
	enum { VERBOSE_BLOCK_IO_PBA         = 0 };
	enum { VERBOSE_CHECK                = 0 };
}

#endif /* _TRESOR__VERBOSITY_H_ */
