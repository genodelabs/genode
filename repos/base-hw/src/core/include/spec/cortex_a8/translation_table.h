/*
 * \brief Translation table definitions for core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date 2012-02-22
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__CORTEX_A8__TRANSLATION_TABLE_H_
#define _CORE__INCLUDE__SPEC__CORTEX_A8__TRANSLATION_TABLE_H_

/* core includes */
#include <short_translation_table.h>

constexpr unsigned Genode::Translation::_device_tex() { return 2; }

#endif /* _CORE__INCLUDE__SPEC__CORTEX_A8__TRANSLATION_TABLE_H_ */
