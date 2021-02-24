/*
 * \brief  Local extension of Genodes UTF8 utilities
 * \author Norman Feske
 * \author Martin Stein
 * \date   2021-03-04
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UTF8_H_
#define _UTF8_H_

/* Genode includes */
#include <util/utf8.h>

/* local includes */
#include <types.h>

namespace File_vault {

	enum {
		CODEPOINT_BACKSPACE = 8,
		CODEPOINT_TAB       = 9,
	};
}

#endif /* _UTF8_H_ */
